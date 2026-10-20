cmake_minimum_required(VERSION 3.20)
project(lights_timer_tests LANGUAGES CXX)
enable_testing()
set(CMAKE_CXX_STANDARD 17)

set(JH_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../JaszczurHAL"
    CACHE PATH "JaszczurHAL source tree")
set(JH_HOST_BUILD "${JH_ROOT}/.build/lights-timer-kv"
    CACHE PATH "Host build containing hal_mock and jh_bearssl_host")
foreach(name hal_mock jh_bearssl_host)
    if(NOT EXISTS "${JH_HOST_BUILD}/lib${name}.a")
        message(FATAL_ERROR "Build ${name} in ${JH_HOST_BUILD} first; see TimerNTP/README.md")
    endif()
endforeach()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
# HAL headers include jhbase (hal_status.h, hal_array.h).
include("${JH_ROOT}/cmake/jh_jhbase.cmake")
jh_jhbase_include_dir(JH_JHBASE_INCLUDE)
set(APP "${CMAKE_CURRENT_SOURCE_DIR}/../TimerNTP")
# Keep the production RP storage path; replace only physical flash operations.
add_library(rp_storage OBJECT
    "${JH_ROOT}/src/hal/storage/hal_eeprom.cpp"
    "${JH_ROOT}/src/hal/storage/hal_kv.cpp"
    "${JH_ROOT}/src/hal/storage/jh_eeprom_flash_provider.cpp"
    "${JH_ROOT}/src/hal/impl/rp2040/hal_eeprom.cpp"
    "${JH_ROOT}/src/hal/impl/rp2040/drivers/flash/rp_flash_storage.cpp")
target_include_directories(rp_storage PRIVATE
    "${JH_ROOT}/tests/fixtures/rp_flash" "${JH_ROOT}/src" "${JH_JHBASE_INCLUDE}")
target_compile_definitions(rp_storage PRIVATE
    HAL_TARGET_RP2040=1 HAL_ENABLE_KV=1 HAL_BOARD_PROFILE_RP_PICO_W=1
    HAL_RP_FLASH_EEPROM_SIZE=8192 PICO_FLASH_SIZE_BYTES=2097152)

target_compile_options(rp_storage PRIVATE -Wall -Wextra -Werror)

foreach(case default alternate)
    set(target test_schedule_${case})
    set(GENERATED "${CMAKE_CURRENT_BINARY_DIR}/${case}/hardware")
    set(config "${APP}")
    if(case STREQUAL "alternate")
        set(config "${CMAKE_CURRENT_BINARY_DIR}/alternate/config")
        execute_process(COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/generate_gpio_fixture.py"
            "${APP}/device_tree.json" "${config}"
            COMMAND_ERROR_IS_FATAL ANY)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${APP}/device_tree.json" "${CMAKE_CURRENT_SOURCE_DIR}/generate_gpio_fixture.py")
    endif()
    add_custom_target(${target}_hardware
        COMMAND "${Python3_EXECUTABLE}" "${JH_ROOT}/scripts/generate_hardware_config.py"
            resolve --config-dir "${config}" --assembly picow --output-dir "${GENERATED}"
            --output-root "${CMAKE_CURRENT_BINARY_DIR}"
        BYPRODUCTS "${GENERATED}/jh_hardware.h"
        VERBATIM)
    add_executable(${target} test_schedule_storage.cpp
        "${APP}/MyHardware.cpp" "${APP}/MQTTClient.cpp"
        "${APP}/NTPMachine.cpp" "${APP}/OledDisplayFlow.cpp"
        "${APP}/CredentialValues.cpp" "${APP}/watchdog.cpp"
        "${APP}/OTAUpdates.cpp" "${APP}/DiscoverMe.cpp"
        $<TARGET_OBJECTS:rp_storage>)
    add_dependencies(${target} ${target}_hardware)
    target_include_directories(${target} PRIVATE
        "${APP}" "${CMAKE_CURRENT_SOURCE_DIR}/../Credentials/src"
        "${JH_ROOT}/src" "${JH_JHBASE_INCLUDE}" "${JH_ROOT}/third_party/cJSON"
        "${JH_ROOT}/tests/fixtures/rp_flash")
    target_compile_definitions(${target} PRIVATE
        # Mock GPIO has no CYW43 LED pin; use an unused mock GPIO for it.
        HAL_TARGET_MOCK=1 LED_BUILTIN=29
        # The shared mock header also declares controls for these modules.
        HAL_ENABLE_MCP2515=1 HAL_ENABLE_PWM_FREQ=1 HAL_ENABLE_RGB_LED=1)
    target_compile_options(${target} PRIVATE
        "SHELL:-include '${APP}/hal_project_config.h'"
        "SHELL:-include '${GENERATED}/jh_hardware.h'")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(${target} PRIVATE
        "${JH_HOST_BUILD}/libhal_mock.a" "${JH_HOST_BUILD}/libjh_bearssl_host.a")
    add_test(NAME ${target} COMMAND ${target})
endforeach()
