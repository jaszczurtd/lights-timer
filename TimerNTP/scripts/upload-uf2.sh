#!/usr/bin/env bash
# =============================================================================
# BOOTSEL (UF2) upload helper
# Compiles the project and copies .uf2 to mounted BOOTSEL storage
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
CMAKE_BUILD_DIR="$PROJECT_DIR/.build/cmake"
UF2_FILE="$PROJECT_DIR/.build/firmware.uf2"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

info()  { echo -e "${CYAN}[INFO]${NC} $*"; }
ok()    { echo -e "${GREEN}[OK]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
err()   { echo -e "${RED}[ERROR]${NC} $*"; }

# Compile through the CMake buildsystem.
info "Configuring CMake..."
"$SCRIPT_DIR/configure-cmake.sh"

info "Compiling firmware..."
if ! cmake --build "$CMAKE_BUILD_DIR" --target firmware; then
    err "Compilation failed"
    exit 1
fi

# Find UF2 artifact
echo ""
info "Searching for UF2 file..."
UF2="$UF2_FILE"

if [[ ! -f "$UF2" ]]; then
    err "No firmware.uf2 file found at $UF2"
    exit 1
fi

ok "Found: $UF2"

# Find BOOTSEL drive
info "Searching for BOOTSEL drive..."
MOUNT=""
for name in RPI-RP2 RP2350 RPI-RP2350; do
    MOUNT=$(find /media/"$USER" -maxdepth 1 -name "$name" -type d 2>/dev/null | head -1)
    if [[ -n "$MOUNT" ]]; then
        break
    fi
done

if [[ -z "$MOUNT" ]]; then
    # Also check /run/media (some distros)
    for name in RPI-RP2 RP2350 RPI-RP2350; do
        MOUNT=$(find /run/media/"$USER" -maxdepth 1 -name "$name" -type d 2>/dev/null | head -1)
        if [[ -n "$MOUNT" ]]; then
            break
        fi
    done
fi

if [[ -z "$MOUNT" ]]; then
    err "BOOTSEL drive not found"
    echo ""
    echo "  Instructions:"
    echo "  1. Unplug the board from USB"
    echo "  2. Hold the BOOTSEL button"
    echo "  3. Plug USB in while holding BOOTSEL"
    echo "  4. Release BOOTSEL - an RPI-RP2 drive should appear"
    echo "  5. Run this script again"
    echo ""
    echo "  Mounted drives in /media/$USER/:"
    ls /media/"$USER"/ 2>/dev/null || echo "    (none)"
    exit 1
fi

# Copy UF2
info "Copying to $MOUNT..."
cp "$UF2" "$MOUNT/"
sync

echo ""
ok "UF2 upload finished"
ok "File: $(basename "$UF2") -> $MOUNT/"
