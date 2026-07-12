#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TARGET=${1:-}

case "${TARGET}" in
    rp2040)
        if [[ -n "${RP2040_TOOLCHAIN_BIN:-}" ]]; then
            TOOLCHAIN_BIN=${RP2040_TOOLCHAIN_BIN}
        else
            DATA_DIR=${ARDUINO_DATA_DIR:-"${HOME}/.arduino15"}
            TOOLCHAIN_BIN=$(find "${DATA_DIR}/packages/rp2040/tools/pqt-gcc" \
                -mindepth 3 -maxdepth 3 -type f -name arm-none-eabi-gcc \
                -path '*/bin/*' -printf '%h\n' 2>/dev/null | sort -V | tail -n 1)
        fi
        ;;
    stm32g474)
        if [[ -n "${STM32_TOOLCHAIN_BIN:-}" ]]; then
            TOOLCHAIN_BIN=${STM32_TOOLCHAIN_BIN}
        else
            COMPILER=$(command -v arm-none-eabi-gcc || true)
            TOOLCHAIN_BIN=${COMPILER%/*}
        fi
        ;;
    *)
        echo "Usage: $0 <rp2040|stm32g474>" >&2
        exit 2
        ;;
esac

if [[ -z "${TOOLCHAIN_BIN:-}" || ! -x "${TOOLCHAIN_BIN}/arm-none-eabi-gcc" ]]; then
    echo "error: ARM toolchain not found for ${TARGET}" >&2
    exit 2
fi

export CREDENTIALS_TOOLCHAIN_BIN=${TOOLCHAIN_BIN}
BUILD_DIR="${ROOT_DIR}/build/${TARGET}"
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_TOOLCHAIN_FILE="${ROOT_DIR}/cmake/arm-none-eabi-toolchain.cmake" \
    -DCREDENTIALS_TARGET="${TARGET}"
cmake --build "${BUILD_DIR}" --target Credentials
