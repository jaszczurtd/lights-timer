#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SOURCE_DIR="${PROJECT_DIR}/libraries/Credentials"
DESTINATION=${1:-"${PROJECT_DIR}/../libraries/Credentials"}

if [[ -e "${DESTINATION}" ]]; then
    echo "error: refusing to overwrite existing Credentials: ${DESTINATION}" >&2
    echo "The current private library remains untouched." >&2
    exit 2
fi

mkdir -p "$(dirname "${DESTINATION}")"
cp -a "${SOURCE_DIR}" "${DESTINATION}"
"${DESTINATION}/scripts/configure.sh"

echo "Installed Credentials template at ${DESTINATION}"
echo "See ${DESTINATION}/README.md before building it."
