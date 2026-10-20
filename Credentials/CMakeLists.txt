cmake_minimum_required(VERSION 3.20)

# scripts/build.py configures this project with the toolchain of the
# JaszczurHAL firmware build of the target, so the archive has the firmware's
# ABI: the Pico SDK choice for PICO_PLATFORM, the HAL STM32 toolchain file or
# the ESP-IDF one.
set(CREDENTIALS_TARGET "" CACHE STRING "JaszczurHAL target id")
set(CREDENTIALS_JASZCZURHAL_ROOT "" CACHE PATH
    "Path to the JaszczurHAL source tree")
set(CREDENTIALS_ARCHIVE_DIR "" CACHE PATH
    "Directory that receives libCredentials.a")

if(CREDENTIALS_TARGET STREQUAL "" OR CREDENTIALS_ARCHIVE_DIR STREQUAL "")
    message(FATAL_ERROR "Build the archive with scripts/build.py <target>")
endif()
if(NOT EXISTS
   "${CREDENTIALS_JASZCZURHAL_ROOT}/src/hal/core/hal_array.h")
    message(FATAL_ERROR
        "Set CREDENTIALS_JASZCZURHAL_ROOT to the JaszczurHAL source tree")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config/CredentialsData.local.h" OR
   NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config/MacHostMapping.local.cpp")
    message(FATAL_ERROR "Run scripts/configure.py and fill the generated config/*.local.* files")
endif()

if(DEFINED PICO_PLATFORM)
    # Before project() the SDK only selects the platform and its toolchain,
    # as in a firmware build; the SDK itself is never initialized here.
    include("${CREDENTIALS_JASZCZURHAL_ROOT}/third_party/pico-sdk/pico_sdk_init.cmake")
endif()

project(Credentials LANGUAGES C CXX)

add_library(Credentials STATIC
    Credentials.cpp
    config/MacHostMapping.local.cpp
)
target_include_directories(Credentials PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
# hal_array.h includes jhbase; HAL's module selects its checkout.
include("${CREDENTIALS_JASZCZURHAL_ROOT}/cmake/jh_jhbase.cmake")
jh_jhbase_include_dir(CREDENTIALS_JHBASE_INCLUDE)
target_include_directories(Credentials PRIVATE
    "${CREDENTIALS_JASZCZURHAL_ROOT}/src"
    "${CREDENTIALS_JHBASE_INCLUDE}"
)
target_compile_features(Credentials PRIVATE cxx_std_17)
target_compile_options(Credentials PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)

set_target_properties(Credentials PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CREDENTIALS_ARCHIVE_DIR}"
    OUTPUT_NAME Credentials
)

if(DEFINED CREDENTIALS_IDF_TARGET)
    # ESP-IDF adds its compiler flags (-mlongcalls, the C library specs and
    # others) only through its build system. Processing the C library
    # component with the default configuration, as JaszczurHAL builds use it,
    # gives the archive the flags and headers of a firmware build; the
    # archive still holds only the Credentials objects.
    include("$ENV{IDF_PATH}/tools/cmake/idf.cmake")
    idf_build_process("${CREDENTIALS_IDF_TARGET}"
        COMPONENTS esp_libc
        SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig"
        BUILD_DIR "${CMAKE_BINARY_DIR}"
    )
    target_link_libraries(Credentials PRIVATE idf::esp_libc)
endif()
