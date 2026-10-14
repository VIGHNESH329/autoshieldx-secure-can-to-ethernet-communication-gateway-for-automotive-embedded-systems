#include "mcp2515.h"
#include <stdio.h>
#include <string.h>

static inline void cs_select(void) {
    gpio_put(PICO_CAN_CS, 0);
    sleep_us(1);
}

static inline void cs_deselect(void) {
    sleep_us(1);
    gpio_put(PICO_CAN_CS, 1);
    sleep_us(1);
}

static void mcp2515_reset(void) {
    cs_select();
    uint8_t cmd = MCP_RESET;
    spi_write_blocking(PICO_CAN_SPI, &cmd, 1);
    cs_deselect();
    sleep_ms(10); // Wait for oscillator startup
}

static uint8_t mcp2515_read_reg(uint8_t reg) {
    cs_select();
    uint8_t tx[2] = { MCP_READ, reg };
    spi_write_blocking(PICO_CAN_SPI, tx, 2);
    uint8_t val = 0;
    spi_read_blocking(PICO_CAN_SPI, 0x00, &val, 1);
    cs_deselect();
    return val;
}

static void mcp2515_write_reg(uint8_t reg, uint8_t val) {
    cs_select();
    uint8_t tx[3] = { MCP_WRITE, reg, val };
    spi_write_blocking(PICO_CAN_SPI, tx, 3);
    cs_deselect();
}

static void mcp2515_bit_modify(uint8_t reg, uint8_t mask, uint8_t val) {
    cs_select();
    uint8_t tx[4] = { MCP_BITMOD, reg, mask, val };
    spi_write_blocking(PICO_CAN_SPI, tx, 4);
    cs_deselect();
}

uint8_t mcp2515_read_status(void) {
    cs_select();
    uint8_t cmd = MCP_READ_STATUS;
    spi_write_blocking(PICO_CAN_SPI, &cmd, 1);
    uint8_t status = 0;
    spi_read_blocking(PICO_CAN_SPI, 0x00, &status, 1);
    cs_deselect();
    return status;
}

static bool mcp2515_set_mode(uint8_t mode) {
    mcp2515_bit_modify(MCP_CANCTRL, 0xE0, mode);
    // Poll CANSTAT to confirm mode transition (timeout after 100ms)
    for (int i = 0; i < 100; ++i) {
        uint8_t stat = mcp2515_read_reg(MCP_CANSTAT);
        if ((stat & 0xE0) == mode) {
            return true;
        }
        sleep_ms(1);
    }
    return false;
}

bool mcp2515_init(bool crystal_8mhz) {
    // 1. Configure Hardware SPI0 pins
    spi_init(PICO_CAN_SPI, 10000000); // 10 MHz SPI Clock
    gpio_set_function(PICO_CAN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PICO_CAN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PICO_CAN_MOSI, GPIO_FUNC_SPI);

    // 2. Configure CS Pin (Active Low)
    gpio_init(PICO_CAN_CS);
    gpio_set_dir(PICO_CAN_CS, GPIO_OUT);
    gpio_put(PICO_CAN_CS, 1);
    sleep_ms(5);

    // 3. Reset MCP2515
    mcp2515_reset();

    // 4. Confirm Configuration Mode entered
    if (!mcp2515_set_mode(MODE_CONFIG)) {
        printf("[MCP2515] ERROR: Failed to enter Configuration Mode. Check wiring.\n");
        return false;
    }

    // 5. Configure Bit Timing for 500 kbps (CAN standard automotive bus speed)
    if (crystal_8mhz) {
        // 8 MHz crystal oscillator:
        // Baud = 500 kbps, 8 Tq total (Sync=1, Prop=1, Phase1=3, Phase2=3)
        // BRP = 0, CNF1 = 0x00
        // CNF2 = 0x90 (SAM=1, BTLMODE=1, PHSEG1=2, PRSEG=0)
        // CNF3 = 0x82 (SOF=1, WAKFIL=0, PHSEG2=2)
        mcp2515_write_reg(MCP_CNF1, 0x00);
        mcp2515_write_reg(MCP_CNF2, 0x90);
        mcp2515_write_reg(MCP_CNF3, 0x82);
    } else {
        // 16 MHz crystal oscillator:
        // Baud = 500 kbps, 16 Tq total
        // BRP = 0, CNF1 = 0x00
        // CNF2 = 0xB1
        // CNF3 = 0x85
        mcp2515_write_reg(MCP_CNF1, 0x00);
        mcp2515_write_reg(MCP_CNF2, 0xB1);
        mcp2515_write_reg(MCP_CNF3, 0x85);
    }

    // 6. Disable TX buffer interrupts and rollover
    mcp2515_write_reg(MCP_CANINTE, 0x00);

    // 7. Transition to Normal Operation Mode
    if (!mcp2515_set_mode(MODE_NORMAL)) {
        printf("[MCP2515] ERROR: Failed to enter Normal Mode.\n");
        return false;
    }

    printf("[MCP2515] Initialized successfully in NORMAL mode at 500 kbps (%s crystal).\n",
           crystal_8mhz ? "8 MHz" : "16 MHz");
    return true;
}

bool mcp2515_send_frame(uint32_t can_id, uint8_t dlc, const uint8_t *data) {
    if (dlc > 8) dlc = 8;

    // Check if TXB0 is busy (TXREQ bit set in TXB0CTRL)
    uint8_t ctrl = mcp2515_read_reg(MCP_TXB0CTRL);
    if (ctrl & 0x08) {
        // Wait up to 5ms for pending transmission
        bool freed = false;
        for (int i = 0; i < 50; ++i) {
            sleep_us(100);
            if (!(mcp2515_read_reg(MCP_TXB0CTRL) & 0x08)) {
                freed = true;
                break;
            }
        }
        if (!freed) {
            // Abort pending transmission to prevent bus hang
            mcp2515_write_reg(MCP_TXB0CTRL, 0x00);
            return false;
        }
    }

    // Standard 11-bit CAN ID configuration
    // SIDH = ID[10..3]
    // SIDL = ID[2..0] << 5
    uint8_t sidh = (uint8_t)((can_id >> 3) & 0xFF);
    uint8_t sidl = (uint8_t)((can_id & 0x07) << 5);

    cs_select();
    uint8_t tx_header[5] = {
        MCP_WRITE,
        MCP_TXB0SIDH,
        sidh,
        sidl,
        0x00 // EID8
    };
    spi_write_blocking(PICO_CAN_SPI, tx_header, 5);
    cs_deselect();

    // Write DLC and Data bytes
    cs_select();
    uint8_t tx_data_header[2] = { MCP_WRITE, MCP_TXB0DLC };
    spi_write_blocking(PICO_CAN_SPI, tx_data_header, 2);

    uint8_t payload_buf[9];
    payload_buf[0] = dlc;
    if (data && dlc > 0) {
        memcpy(&payload_buf[1], data, dlc);
    }
    spi_write_blocking(PICO_CAN_SPI, payload_buf, 1 + dlc);
    cs_deselect();

    // Trigger transmission using RTS command on TXB0
    cs_select();
    uint8_t rts_cmd = MCP_RTS_TX0;
    spi_write_blocking(PICO_CAN_SPI, &rts_cmd, 1);
    cs_deselect();

    return true;
}
