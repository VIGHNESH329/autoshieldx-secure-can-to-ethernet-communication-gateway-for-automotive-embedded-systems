cmake_minimum_required(VERSION 3.14)
project(AutoShieldX VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# OpenSSL dependency lookup
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    message(STATUS "[AutoShieldX] OpenSSL found (Version: ${OPENSSL_VERSION}). Linking OpenSSL::Crypto.")
    add_compile_definitions(AUTOSHIELDX_HAS_OPENSSL=1)
else()
    message(STATUS "[AutoShieldX] OpenSSL not found in environment paths. Using built-in RFC 2104 SHA-256/HMAC engine.")
endif()

# Core static library sources
set(CORE_SOURCES
    src/can/can_frame.cpp
    src/can/can_channel.cpp
    src/can/can_transport.cpp
    src/ecu/sensor_interface.cpp
    src/ecu/ecu_simulator.cpp
    src/security/crypto_hmac.cpp
    src/security/replay_protection.cpp
    src/validation/validation_engine.cpp
    src/ml/ml_detector.cpp
    src/networking/ethernet_forwarder.cpp
    src/networking/aws_logger.cpp
    src/gateway/gateway_logger.cpp
    src/gateway/security_pipeline.cpp
)

add_library(autoshieldx_core STATIC ${CORE_SOURCES})
target_include_directories(autoshieldx_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(OpenSSL_FOUND)
    target_link_libraries(autoshieldx_core PUBLIC OpenSSL::Crypto)
endif()

# Platform specific networking libraries
if(WIN32)
    target_link_libraries(autoshieldx_core PUBLIC ws2_32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(autoshieldx_core PUBLIC Threads::Threads)
endif()

# 1. ECU Simulator Executable
add_executable(ecu_simulator src/ecu/main_ecu.cpp)
target_link_libraries(ecu_simulator PRIVATE autoshieldx_core)

# 2. Secure Gateway Executable
add_executable(secure_gateway src/gateway/main_gateway.cpp)
target_link_libraries(secure_gateway PRIVATE autoshieldx_core)

# 3. Laboratory Attack Simulators
add_executable(attack_injection attacks/injection/attack_injection.cpp)
target_link_libraries(attack_injection PRIVATE autoshieldx_core)

add_executable(attack_replay attacks/replay/attack_replay.cpp)
target_link_libraries(attack_replay PRIVATE autoshieldx_core)

add_executable(attack_dos attacks/dos/attack_dos.cpp)
target_link_libraries(attack_dos PRIVATE autoshieldx_core)

add_executable(attack_fuzzy attacks/fuzzy/attack_fuzzy.cpp)
target_link_libraries(attack_fuzzy PRIVATE autoshieldx_core)

add_executable(attack_false_data attacks/false_data/attack_false_data.cpp)
target_link_libraries(attack_false_data PRIVATE autoshieldx_core)

# 4. Unit & Integration Test Suite
add_executable(autoshieldx_tests tests/test_autoshieldx.cpp)
target_link_libraries(autoshieldx_tests PRIVATE autoshieldx_core)

enable_testing()
add_test(NAME AutoShieldX_AllTests COMMAND autoshieldx_tests)

# Output build artifacts to bin/ directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
