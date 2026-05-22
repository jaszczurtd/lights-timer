#!/usr/bin/env bash
# =============================================================================
# Serial Monitor
# Connects to the board serial port
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
SETTINGS_FILE="$PROJECT_DIR/.vscode/settings.json"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# Load settings
read_setting() {
    python3 -c "
import json
with open('$SETTINGS_FILE') as f:
    s = json.load(f)
print(s.get('$1', '${2:-}'))
" 2>/dev/null
}

PORT=$(read_setting "arduino.uploadPort" "/dev/ttyACM0")
BAUD="115200"

echo -e "${CYAN}Serial Monitor${NC}"
echo -e "  Port: ${GREEN}$PORT${NC}"
echo -e "  Baud: ${GREEN}$BAUD${NC}"
echo ""

# Check if the port exists
if [[ ! -e "$PORT" ]]; then
    echo -e "${RED}Port $PORT does not exist${NC}"
    echo ""
    echo "Available ports:"
    ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || echo "  (none)"
    exit 1
fi

# Check if the port is busy
if fuser "$PORT" &>/dev/null; then
    echo -e "${YELLOW}Port $PORT is busy (used by another process):${NC}"
    fuser -v "$PORT" 2>&1 || true
    echo ""
    read -rp "Kill processes blocking the port? [y/N] " kill_answer
    if [[ "$kill_answer" =~ ^[tTyY]$ ]]; then
        fuser -k "$PORT" 2>/dev/null || true
        sleep 1
    else
        exit 1
    fi
fi

# Choose connection tool
# Priority: cat (simplest), screen, minicom
echo -e "Connecting... (${YELLOW}Ctrl+C${NC} to exit)"
echo "----------------------------------------"

# Configure the port
stty -F "$PORT" "$BAUD" cs8 -cstopb -parenb raw -echo 2>/dev/null || true

# Trap Ctrl+C to clean up
cleanup() {
    echo ""
    echo -e "${CYAN}Disconnected.${NC}"
    exit 0
}
trap cleanup INT TERM

# Read from port - cat works best in VS Code terminal
cat "$PORT"
