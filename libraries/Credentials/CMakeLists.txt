cmake_minimum_required(VERSION 3.20)

project(Credentials LANGUAGES C CXX)

set(CREDENTIALS_TARGET "" CACHE STRING "Target ABI: rp2040 or stm32g474")
set_property(CACHE CREDENTIALS_TARGET PROPERTY STRINGS rp2040 stm32g474)

if(NOT CREDENTIALS_TARGET MATCHES "^(rp2040|stm32g474)$")
    message(FATAL_ERROR "Set CREDENTIALS_TARGET to rp2040 or stm32g474")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config/CredentialsData.local.h" OR
   NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config/MacHostMapping.local.cpp")
    message(FATAL_ERROR "Run ./scripts/configure.sh and fill the generated config/*.local.* files")
endif()

add_library(Credentials STATIC
    Credentials.cpp
    config/MacHostMapping.local.cpp
)
target_include_directories(Credentials PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_features(Credentials PRIVATE cxx_std_17)
target_compile_options(Credentials PRIVATE
    -ffunction-sections
    -fdata-sections
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)

if(CREDENTIALS_TARGET STREQUAL "rp2040")
    target_compile_options(Credentials PRIVATE -mcpu=cortex-m0plus -mthumb -Os)
    set(_archive_dir "${CMAKE_CURRENT_SOURCE_DIR}/src/cortex-m0plus")
else()
    target_compile_options(Credentials PRIVATE
        -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -Os)
    set(_archive_dir "${CMAKE_CURRENT_SOURCE_DIR}/build/stm32g474")
endif()

set_target_properties(Credentials PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${_archive_dir}"
    OUTPUT_NAME Credentials
)
