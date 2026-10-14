#ifndef ARDUINO_MCP2515_H
#define ARDUINO_MCP2515_H

#include <Arduino.h>
#include <SPI.h>

// MCP2515 SPI Instructions
#define MCP_RESET       0xC0
#define MCP_READ        0x03
#define MCP_WRITE       0x02
#define MCP_RTS_TX0     0x81
#define MCP_BITMOD      0x05
#define MCP_READ_STATUS 0xA0

// MCP2515 Register Addresses
#define MCP_CANSTAT     0x0E
#define MCP_CANCTRL     0x0F
#define MCP_CNF3        0x28
#define MCP_CNF2        0x29
#define MCP_CNF1        0x2A
#define MCP_CANINTE     0x2B
#define MCP_TXB0CTRL    0x30
#define MCP_TXB0SIDH    0x31
#define MCP_TXB0SIDL    0x32
#define MCP_TXB0DLC     0x35
#define MCP_TXB0D0      0x36

// Operation Modes
#define MODE_NORMAL     0x00
#define MODE_SLEEP      0x20
#define MODE_LOOPBACK   0x40
#define MODE_LISTENONLY 0x60
#define MODE_CONFIG     0x80

class MCP2515_Driver {
public:
    MCP2515_Driver(uint8_t cs_pin);

    /**
     * @brief Initialize MCP2515 CAN controller at 500 kbps.
     * @param crystal_8mhz true for 8 MHz crystal oscillator, false for 16 MHz.
     * @return true on successful initialization into NORMAL mode.
     */
    bool begin(bool crystal_8mhz = true);

    /**
     * @brief Transmit a standard 11-bit CAN 2.0B frame.
     * @param can_id 11-bit CAN ID (e.g. 0x100 or 0x101)
     * @param dlc Data length (0 to 8)
     * @param data Pointer to payload bytes
     * @return true if successfully queued and transmitted.
     */
    bool sendFrame(uint32_t can_id, uint8_t dlc, const uint8_t *data);

    /**
     * @brief Read MCP2515 status register.
     */
    uint8_t readStatus();

    /**
     * @brief Read a register value.
     */
    uint8_t readRegister(uint8_t reg);

    /**
     * @brief Write a register value.
     */
    void writeRegister(uint8_t reg, uint8_t val);

    /**
     * @brief Modify bits in register using bit modify mask.
     */
    void modifyRegister(uint8_t reg, uint8_t mask, uint8_t val);

private:
    uint8_t _cs_pin;

    inline void select() {
        digitalWrite(_cs_pin, LOW);
    }

    inline void deselect() {
        digitalWrite(_cs_pin, HIGH);
    }

    bool setMode(uint8_t mode);
    void reset();
};

#endif // ARDUINO_MCP2515_H
