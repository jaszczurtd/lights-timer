set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(NOT DEFINED ENV{CREDENTIALS_TOOLCHAIN_BIN})
    message(FATAL_ERROR "CREDENTIALS_TOOLCHAIN_BIN is not set")
endif()

set(_toolchain_bin "$ENV{CREDENTIALS_TOOLCHAIN_BIN}")
set(CMAKE_C_COMPILER "${_toolchain_bin}/arm-none-eabi-gcc")
set(CMAKE_CXX_COMPILER "${_toolchain_bin}/arm-none-eabi-g++")
set(CMAKE_AR "${_toolchain_bin}/arm-none-eabi-ar")
set(CMAKE_RANLIB "${_toolchain_bin}/arm-none-eabi-ranlib")
