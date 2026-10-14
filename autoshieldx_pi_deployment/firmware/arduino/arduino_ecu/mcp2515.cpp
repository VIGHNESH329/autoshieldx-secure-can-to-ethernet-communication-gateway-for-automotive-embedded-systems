#include "mcp2515.h"

MCP2515_Driver::MCP2515_Driver(uint8_t cs_pin) : _cs_pin(cs_pin) {
}

void MCP2515_Driver::reset() {
    select();
    SPI.transfer(MCP_RESET);
    deselect();
    delay(10); // Wait for oscillator startup
}

uint8_t MCP2515_Driver::readRegister(uint8_t reg) {
    select();
    SPI.transfer(MCP_READ);
    SPI.transfer(reg);
    uint8_t val = SPI.transfer(0x00);
    deselect();
    return val;
}

void MCP2515_Driver::writeRegister(uint8_t reg, uint8_t val) {
    select();
    SPI.transfer(MCP_WRITE);
    SPI.transfer(reg);
    SPI.transfer(val);
    deselect();
}

void MCP2515_Driver::modifyRegister(uint8_t reg, uint8_t mask, uint8_t val) {
    select();
    SPI.transfer(MCP_BITMOD);
    SPI.transfer(reg);
    SPI.transfer(mask);
    SPI.transfer(val);
    deselect();
}

uint8_t MCP2515_Driver::readStatus() {
    select();
    SPI.transfer(MCP_READ_STATUS);
    uint8_t status = SPI.transfer(0x00);
    deselect();
    return status;
}

bool MCP2515_Driver::setMode(uint8_t mode) {
    modifyRegister(MCP_CANCTRL, 0xE0, mode);
    // Poll CANSTAT to confirm mode transition (timeout after 100ms)
    for (int i = 0; i < 100; ++i) {
        uint8_t stat = readRegister(MCP_CANSTAT);
        if ((stat & 0xE0) == mode) {
            return true;
        }
        delay(1);
    }
    return false;
}

bool MCP2515_Driver::begin(bool crystal_8mhz) {
    pinMode(_cs_pin, OUTPUT);
    deselect();

    // Start SPI bus at 10 MHz (Mode 0, MSB first)
    SPI.begin();
    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));

    // Reset MCP2515
    reset();

    // Confirm Configuration Mode entered
    if (!setMode(MODE_CONFIG)) {
        return false;
    }

    // Configure Bit Timing for 500 kbps (Standard AutoShieldX Bitrate)
    if (crystal_8mhz) {
        // 8 MHz crystal oscillator: 500 kbps
        // 8 Tq total (Sync=1, Prop=1, Phase1=3, Phase2=3)
        writeRegister(MCP_CNF1, 0x00);
        writeRegister(MCP_CNF2, 0x90);
        writeRegister(MCP_CNF3, 0x82);
    } else {
        // 16 MHz crystal oscillator: 500 kbps
        // 16 Tq total
        writeRegister(MCP_CNF1, 0x00);
        writeRegister(MCP_CNF2, 0xB1);
        writeRegister(MCP_CNF3, 0x85);
    }

    // Disable interrupts on buffers
    writeRegister(MCP_CANINTE, 0x00);

    // Transition to Normal Operation Mode
    if (!setMode(MODE_NORMAL)) {
        return false;
    }

    return true;
}

bool MCP2515_Driver::sendFrame(uint32_t can_id, uint8_t dlc, const uint8_t *data) {
    if (dlc > 8) dlc = 8;

    // Check if TXB0 is busy (TXREQ bit set in TXB0CTRL)
    uint8_t ctrl = readRegister(MCP_TXB0CTRL);
    if (ctrl & 0x08) {
        // Wait up to 5ms for pending transmission
        bool freed = false;
        for (int i = 0; i < 50; ++i) {
            delayMicroseconds(100);
            if (!(readRegister(MCP_TXB0CTRL) & 0x08)) {
                freed = true;
                break;
            }
        }
        if (!freed) {
            // Abort pending transmission to prevent bus lockup
            writeRegister(MCP_TXB0CTRL, 0x00);
            return false;
        }
    }

    // Standard 11-bit CAN ID setup
    uint8_t sidh = (uint8_t)((can_id >> 3) & 0xFF);
    uint8_t sidl = (uint8_t)((can_id & 0x07) << 5);

    select();
    SPI.transfer(MCP_WRITE);
    SPI.transfer(MCP_TXB0SIDH);
    SPI.transfer(sidh);
    SPI.transfer(sidl);
    SPI.transfer(0x00); // EID8
    SPI.transfer(0x00); // EID0
    SPI.transfer(dlc);
    for (uint8_t i = 0; i < dlc; ++i) {
        SPI.transfer(data[i]);
    }
    deselect();

    // Trigger transmission via RTS command on TXB0
    select();
    SPI.transfer(MCP_RTS_TX0);
    deselect();

    return true;
}
