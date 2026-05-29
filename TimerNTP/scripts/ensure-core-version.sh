#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSION_FILE="$SCRIPT_DIR/core_version.txt"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

info()  { echo -e "${CYAN}[INFO]${NC} $*"; }
ok()    { echo -e "${GREEN}[OK]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
err()   { echo -e "${RED}[ERROR]${NC} $*"; }

usage() {
    cat << EOF
Usage: $0 [--cli <path>] [--fqbn <packager:arch:board[:menu=val...]>] [--core <packager:arch>]

Reads required version from scripts/core_version.txt and ensures matching core is installed.
EOF
}

CLI="arduino-cli"
FQBN=""
CORE=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --cli)
            CLI="${2:-}"
            shift 2
            ;;
        --fqbn)
            FQBN="${2:-}"
            shift 2
            ;;
        --core)
            CORE="${2:-}"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            err "Unknown argument: $1"
            usage
            exit 1
            ;;
    esac
done

if ! command -v "$CLI" >/dev/null 2>&1; then
    err "arduino-cli not found: $CLI"
    exit 1
fi

if [[ ! -f "$VERSION_FILE" ]]; then
    err "Missing version file: $VERSION_FILE"
    exit 1
fi

REQUIRED_VERSION=$(grep -vE '^\s*#' "$VERSION_FILE" | sed 's/^\s*//;s/\s*$//' | grep -v '^$' | head -n 1 || true)
if [[ -z "$REQUIRED_VERSION" ]]; then
    err "No version found in $VERSION_FILE"
    exit 1
fi

if [[ -z "$CORE" ]]; then
    if [[ -n "$FQBN" ]]; then
        IFS=':' read -r pkg arch _ <<< "$FQBN"
        if [[ -n "${pkg:-}" && -n "${arch:-}" ]]; then
            CORE="${pkg}:${arch}"
        fi
    fi
fi

if [[ -z "$CORE" ]]; then
    CORE="rp2040:rp2040"
fi

INSTALLED_VERSION=$("$CLI" core list | awk -v core="$CORE" 'NR > 1 && $1 == core { print $2; exit }')

if [[ "$INSTALLED_VERSION" == "$REQUIRED_VERSION" ]]; then
    ok "Core $CORE already at required version $REQUIRED_VERSION"
    exit 0
fi

if [[ -n "$INSTALLED_VERSION" ]]; then
    warn "Core $CORE is at $INSTALLED_VERSION, required is $REQUIRED_VERSION"
    info "Switching core version..."
    "$CLI" core uninstall "$CORE"
else
    info "Core $CORE is not installed"
fi

info "Updating index..."
"$CLI" core update-index

info "Installing $CORE@$REQUIRED_VERSION ..."
"$CLI" core install "$CORE@$REQUIRED_VERSION"

FINAL_VERSION=$("$CLI" core list | awk -v core="$CORE" 'NR > 1 && $1 == core { print $2; exit }')
if [[ "$FINAL_VERSION" != "$REQUIRED_VERSION" ]]; then
    err "Failed to set required core version. Expected $REQUIRED_VERSION, got ${FINAL_VERSION:-<none>}"
    exit 1
fi

ok "Using core $CORE version $FINAL_VERSION"