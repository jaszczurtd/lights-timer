cmake_minimum_required(VERSION 3.20)

# Standalone convenience wrapper. The VS Code manifest invokes the same
# dispatcher directly, so both entry points use the native Pico SDK backend.
set(JH_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../libraries/JaszczurHAL"
    CACHE PATH "JaszczurHAL repository root")
set(JH_PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
    CACHE PATH "Firmware project directory")
set(JH_MODULE_NAME "TimerNTP" CACHE STRING "Firmware module name")
set(JH_TARGET "rp2040" CACHE STRING "JaszczurHAL target")
set(JH_BOARD "picow" CACHE STRING "JaszczurHAL board profile")
set(PICO_SDK_PATH "${JH_ROOT}/third_party/pico-sdk"
    CACHE PATH "Pico SDK path")
set(JH_EXTRA_DEFINES
    "HAL_ENABLE_OTA;HAL_ENABLE_KV;HAL_ENABLE_LITTLEFS"
    CACHE STRING "Flash-layout feature definitions")
set(JH_OTA_GENERATION 1 CACHE STRING "OTA generation")
set(JH_OTA_VERSION "lights-timer" CACHE STRING "OTA version")
set(JH_EXTRA_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../libraries/Credentials/src"
    CACHE STRING "Additional include directories")
set(JH_LINK_LIBRARIES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../libraries/Credentials/src/cortex-m0plus/libCredentials.a"
    CACHE STRING "Additional link libraries")

add_subdirectory(
    "${JH_ROOT}/cmake/jh_firmware_project"
    "${CMAKE_BINARY_DIR}/jh_firmware")
