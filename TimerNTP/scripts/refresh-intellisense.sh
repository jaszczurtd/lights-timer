#!/usr/bin/env bash
# =============================================================================
# Refresh IntelliSense configuration from the CMake-generated Arduino build.
#
# Strategy:
# 1. Configure CMake from .vscode/settings.json.
# 2. Ask the firmware_compile_db target to generate compile_commands.json.
# 3. Parse compile_commands.json to extract -I, -D, and compiler path.
# 4. Patch generated-sketch source entries back to workspace source paths.
# 5. Generate .vscode/c_cpp_properties.json.
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
SETTINGS_FILE="$PROJECT_DIR/.vscode/settings.json"
CPP_PROPS_FILE="$PROJECT_DIR/.vscode/c_cpp_properties.json"
CMAKE_BUILD_DIR="$PROJECT_DIR/.build/cmake"
BUILD_DIR="$PROJECT_DIR/.build/arduino"
SKETCH_DIR="$CMAKE_BUILD_DIR/sketch/TimerNTP"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

info() { echo -e "${CYAN}[INFO]${NC} $*"; }
ok()   { echo -e "${GREEN}[OK]${NC} $*"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
err()  { echo -e "${RED}[ERROR]${NC} $*" >&2; }

read_setting() {
    local key="$1"
    local default="${2:-}"
    python3 - "$SETTINGS_FILE" "$key" "$default" <<'PYEOF'
import json
import os
import sys

settings_file, key, default = sys.argv[1], sys.argv[2], sys.argv[3]
if not os.path.isfile(settings_file):
    print(default)
    raise SystemExit(0)

with open(settings_file) as f:
    settings = json.load(f)

value = settings.get(key, default)
if isinstance(value, bool):
    value = "true" if value else "false"
print(value)
PYEOF
}

generate_compile_db() {
    info "Configuring CMake..."
    "$SCRIPT_DIR/configure-cmake.sh"

    info "Generating compile_commands.json..."
    if cmake --build "$CMAKE_BUILD_DIR" --target firmware_compile_db; then
        ok "compile_commands.json generated"
    else
        err "Failed to generate compile_commands.json"
        exit 1
    fi

    if [[ ! -f "$BUILD_DIR/compile_commands.json" ]]; then
        err "compile_commands.json does not exist: $BUILD_DIR/compile_commands.json"
        exit 1
    fi
}

generate_cpp_properties() {
    python3 <<'PYEOF'
import json
import os
import shlex

project_dir = os.environ["PROJECT_DIR"]
build_dir = os.environ["BUILD_DIR"]
sketch_dir = os.environ["SKETCH_DIR"]
arduino_sketch_dir = os.path.join(build_dir, "sketch")
settings_path = os.path.join(project_dir, ".vscode", "settings.json")
cpp_props_path = os.path.join(project_dir, ".vscode", "c_cpp_properties.json")
compile_db_path = os.path.join(build_dir, "compile_commands.json")

settings = {}
if os.path.isfile(settings_path):
    with open(settings_path) as f:
        settings = json.load(f)

board_desc = settings.get("arduino.boardDescription", "Arduino-Pico")
sketchbook_path = settings.get("arduino.sketchbookPath", "")

with open(compile_db_path) as f:
    commands = json.load(f)

includes = set()
defines = set()
compiler_path = ""
iprefix = ""


def normalize_path(path, directory=""):
    path = path.strip().strip('"').strip("'")
    if not path:
        return ""
    if not os.path.isabs(path) and directory:
        path = os.path.join(directory, path)
    return os.path.normpath(path)


def process_response_file(resp_file, includes, defines, iprefix, directory=""):
    resp_file = normalize_path(resp_file, directory)
    if not os.path.isfile(resp_file):
        return

    with open(resp_file) as rf:
        for line in rf:
            line = line.strip()
            if not line:
                continue
            if line.startswith("-iwithprefixbefore"):
                rel = line[len("-iwithprefixbefore"):]
                if iprefix and rel:
                    includes.add(os.path.normpath(iprefix + rel))
            elif line.startswith("-I"):
                path = normalize_path(line[2:], directory)
                if path:
                    includes.add(path)
            elif line.startswith("-D"):
                define = line[2:].strip('"').strip("'")
                if define:
                    defines.add(define)


for entry in commands:
    args = entry.get("arguments", [])
    if not args:
        cmd = entry.get("command", "")
        try:
            args = shlex.split(cmd)
        except ValueError:
            args = cmd.split()

    if not args:
        continue

    directory = entry.get("directory", "")

    if not compiler_path and args[0].endswith(("g++", "gcc", "arm-none-eabi-g++", "arm-none-eabi-gcc")):
        compiler_path = args[0]

    i = 0
    while i < len(args):
        arg = args[i]

        if arg.startswith("-I"):
            path = arg[2:] if len(arg) > 2 else (args[i + 1] if i + 1 < len(args) else "")
            path = normalize_path(path, directory)
            if path:
                includes.add(path)
                if len(arg) == 2:
                    i += 1

        elif arg == "-isystem" and i + 1 < len(args):
            path = normalize_path(args[i + 1], directory)
            if path:
                includes.add(path)
            i += 1

        elif arg.startswith("-iprefix"):
            iprefix = arg[8:] if len(arg) > 8 else (args[i + 1] if i + 1 < len(args) else "")
            if len(arg) == 8:
                i += 1

        elif arg.startswith("@") and arg.endswith(".txt"):
            process_response_file(arg[1:], includes, defines, iprefix, directory)

        elif arg.startswith("-D"):
            define = arg[2:] if len(arg) > 2 else (args[i + 1] if i + 1 < len(args) else "")
            define = define.strip('"').strip("'")
            if define:
                defines.add(define)
                if len(arg) == 2:
                    i += 1

        i += 1

if sketchbook_path:
    user_libs = os.path.join(sketchbook_path, "libraries")
    if os.path.isdir(user_libs):
        for lib in os.listdir(user_libs):
            lib_path = os.path.join(user_libs, lib)
            if os.path.isdir(lib_path):
                src = os.path.join(lib_path, "src")
                includes.add(src if os.path.isdir(src) else lib_path)

for path in (project_dir, sketch_dir, build_dir, os.path.join(build_dir, "core")):
    if os.path.isdir(path):
        includes.add(path)

includes_list = sorted(p for p in includes if os.path.isdir(p))
defines_list = sorted(defines)
includes_list.append("${workspaceFolder}/**")

source_map = {}
for generated_dir in (sketch_dir, arduino_sketch_dir):
    if not os.path.isdir(generated_dir):
        continue
    for name in os.listdir(project_dir):
        if name.endswith((".c", ".cpp", ".h", ".hpp")):
            generated = os.path.normpath(os.path.join(generated_dir, name))
            original = os.path.normpath(os.path.join(project_dir, name))
            source_map[generated] = original

real_source_map = {
    os.path.realpath(generated): original
    for generated, original in source_map.items()
    if os.path.exists(generated) and os.path.exists(original)
}


def replace_in_entry(entry, generated, original):
    patched = dict(entry)
    patched["file"] = original

    if "command" in patched:
        patched["command"] = patched["command"].replace(generated, original)

    if "arguments" in patched:
        patched["arguments"] = [
            original if arg == generated else arg.replace(generated, original)
            for arg in patched["arguments"]
        ]

    return patched


patched_commands = list(commands)
patched_count = 0

for entry in commands:
    file_path = os.path.normpath(entry.get("file", ""))
    real_file_path = os.path.realpath(file_path)
    original_path = source_map.get(file_path) or real_source_map.get(real_file_path)

    if original_path and original_path != file_path:
        patched_commands.append(replace_in_entry(entry, file_path, original_path))
        patched_count += 1

patched_db_path = os.path.join(build_dir, "compile_commands_patched.json")
with open(patched_db_path, "w") as f:
    json.dump(patched_commands, f, indent=4)
    f.write("\n")

config = {
    "configurations": [
        {
            "name": board_desc,
            "includePath": includes_list,
            "defines": defines_list,
            "compilerPath": compiler_path if compiler_path and os.path.isfile(compiler_path) else "",
            "compileCommands": patched_db_path,
            "cStandard": "c17",
            "cppStandard": "gnu++17",
            "intelliSenseMode": "gcc-arm-none-eabi"
        }
    ],
    "version": 4
}

os.makedirs(os.path.dirname(cpp_props_path), exist_ok=True)
with open(cpp_props_path, "w") as f:
    json.dump(config, f, indent=4)
    f.write("\n")

print(f"  Include paths:    {len(includes_list) - 1}")
print(f"  Defines:          {len(defines_list)}")
print(f"  Compiler:         {compiler_path or 'not found'}")
print(f"  compile_commands: {patched_db_path} ({patched_count} source path patch(es))")
print(f"  Output:           {cpp_props_path}")
PYEOF
}

main() {
    echo ""
    info "Refreshing IntelliSense configuration..."
    echo ""

    export PROJECT_DIR
    export BUILD_DIR
    export SKETCH_DIR

    local fqbn
    fqbn="$(read_setting "arduino.fqbn" "")"
    if [[ -z "$fqbn" ]]; then
        err "Missing FQBN in settings.json. Run: ./scripts/select-board.sh"
        exit 1
    fi
    info "FQBN: $fqbn"

    echo ""
    generate_compile_db

    echo ""
    info "Generating c_cpp_properties.json..."
    generate_cpp_properties

    echo ""
    ok "IntelliSense configuration refreshed"
    echo ""
    echo "  Next steps in VS Code:"
    echo "  1. Ctrl+Shift+P -> C/C++: Reset IntelliSense Database"
    echo "  2. Ctrl+Shift+P -> Developer: Reload Window"
    echo ""
}

main "$@"
