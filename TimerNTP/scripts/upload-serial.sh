#!/usr/bin/env bash
# =============================================================================
# Serial upload helper for the CMake-based firmware build.
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
CMAKE_BUILD_DIR="$PROJECT_DIR/.build/cmake"
MONITOR="$SCRIPT_DIR/serial-persistent.py"

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'

info() { echo -e "${CYAN}[INFO]${NC} $*"; }
ok()   { echo -e "${GREEN}[OK]${NC} $*"; }
err()  { echo -e "${RED}[ERROR]${NC} $*" >&2; }

if [[ -f "$MONITOR" ]] && ! pgrep -f "serial-persistent.py -m pico" >/dev/null 2>&1; then
    nohup python3 "$MONITOR" -m pico >/tmp/timerntp-persistent-monitor.log 2>&1 &
fi

info "Configuring CMake..."
"$SCRIPT_DIR/configure-cmake.sh"

info "Uploading firmware..."
if ! cmake --build "$CMAKE_BUILD_DIR" --target firmware_upload; then
    err "Upload failed"
    exit 1
fi

ok "Serial upload finished"
