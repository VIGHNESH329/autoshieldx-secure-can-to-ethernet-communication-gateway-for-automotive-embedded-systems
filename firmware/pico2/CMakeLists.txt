cmake_minimum_required(VERSION 3.13)

# Pull in Raspberry Pi Pico SDK
include(pico_sdk_import.cmake)

project(autoshieldx_pico2_ecu C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

add_executable(autoshieldx_pico2_ecu
    main.c
    mcp2515.c
    sha256_hmac.c
    sensor_input.c
)

target_include_directories(autoshieldx_pico2_ecu PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(autoshieldx_pico2_ecu
    pico_stdlib
    hardware_spi
    hardware_gpio
    hardware_timer
)

# Enable standard output over USB Serial CDC (115200 baud)
pico_enable_stdio_usb(autoshieldx_pico2_ecu 1)
pico_enable_stdio_uart(autoshieldx_pico2_ecu 0)

# Generate UF2 bootloader image for drag-and-drop flashing
pico_add_extra_outputs(autoshieldx_pico2_ecu)
