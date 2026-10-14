#pragma once

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// MCP2515 Register Addresses
#define MCP_RXF0SIDH    0x00
#define MCP_RXF0SIDL    0x01
#define MCP_CANSTAT     0x0E
#define MCP_CANCTRL     0x0F
#define MCP_CNF3        0x28
#define MCP_CNF2        0x29
#define MCP_CNF1        0x2A
#define MCP_CANINTE     0x2B
#define MCP_CANINTF     0x2C
#define MCP_EFLG        0x2D
#define MCP_TXB0CTRL    0x30
#define MCP_TXB0SIDH    0x31
#define MCP_TXB0SIDL    0x32
#define MCP_TXB0DLC     0x35
#define MCP_TXB0D0      0x36
#define MCP_RXB0CTRL    0x60
#define MCP_RXB0SIDH    0x61
#define MCP_RXB0DLC     0x65
#define MCP_RXB0D0      0x66

// MCP2515 SPI Commands
#define MCP_RESET       0xC0
#define MCP_READ        0x03
#define MCP_WRITE       0x02
#define MCP_RTS_TX0     0x81
#define MCP_READ_STATUS 0xA0
#define MCP_BITMOD      0x05

// Operation Modes
#define MODE_NORMAL     0x00
#define MODE_SLEEP      0x20
#define MODE_LOOPBACK   0x40
#define MODE_LISTENONLY 0x60
#define MODE_CONFIG     0x80

// Pin Configuration for Pico 2 (RP2350) Baseline
#define PICO_CAN_SPI    spi0
#define PICO_CAN_MISO   16  // GP16 -> MCP2515 SO
#define PICO_CAN_CS     17  // GP17 -> MCP2515 CS
#define PICO_CAN_SCK    18  // GP18 -> MCP2515 SCK
#define PICO_CAN_MOSI   19  // GP19 -> MCP2515 SI

/**
 * @brief Initialize MCP2515 CAN controller over SPI0.
 * @param crystal_8mhz Set true if breakout board has 8MHz crystal (most common), false for 16MHz.
 * @return true if communication succeeded and device entered Normal CAN mode.
 */
bool mcp2515_init(bool crystal_8mhz);

/**
 * @brief Transmit an 8-byte standard CAN frame.
 * @param can_id 11-bit CAN Identifier (e.g. 0x100)
 * @param dlc Data length code (up to 8)
 * @param data Data bytes (up to 8)
 * @return true if successfully queued and transmitted.
 */
bool mcp2515_send_frame(uint32_t can_id, uint8_t dlc, const uint8_t *data);

/**
 * @brief Read CAN status register.
 */
uint8_t mcp2515_read_status(void);

#ifdef __cplusplus
}
#endif
