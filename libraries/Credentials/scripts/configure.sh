#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

if [[ ! -e "${ROOT_DIR}/config/CredentialsData.local.h" ]]; then
    cp "${ROOT_DIR}/config/CredentialsData.example.h" \
       "${ROOT_DIR}/config/CredentialsData.local.h"
fi
if [[ ! -e "${ROOT_DIR}/config/MacHostMapping.local.cpp" ]]; then
    cp "${ROOT_DIR}/config/MacHostMapping.example.cpp" \
       "${ROOT_DIR}/config/MacHostMapping.local.cpp"
fi

echo "Created local configuration files under ${ROOT_DIR}/config"
echo "Fill them with your values and set CREDENTIALS_LOCAL_CONFIGURED to 1."
