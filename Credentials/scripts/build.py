#!/usr/bin/env python3
"""Build the Credentials archive for one JaszczurHAL target.

The archive is compiled with the toolchain of the JaszczurHAL firmware build
of that target, taken from the JaszczurHAL board registry: the Pico SDK
toolchain chosen for the target's PICO_PLATFORM, the HAL STM32 toolchain file
or the ESP-IDF toolchain file of the chip. Its ABI therefore matches the
firmware that links it.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any

ARM_TOOLS = ("gcc", "g++", "ar", "ranlib")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "target",
        help="JaszczurHAL target id, e.g. rp2040, rp2350-arm, rp2350-riscv, "
        "stm32g474, esp32 or esp32s3",
    )
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--cmake", type=Path)
    parser.add_argument("--ninja", type=Path)
    parser.add_argument(
        "--toolchain-bin",
        type=Path,
        help="Directory with the GNU Arm or RISC-V compiler (not used by ESP targets).",
    )
    parser.add_argument("--host-environment", type=Path)
    parser.add_argument("--jaszczurhal-root", type=Path)
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print resolved commands as JSON without executing them.",
    )
    return parser.parse_args()


def load_host_environment(root_dir: Path, explicit: Path | None) -> dict[str, Any]:
    candidates: list[Path] = []
    if explicit is not None:
        candidates.append(explicit)
    if os.environ.get("JH_HOST_ENVIRONMENT"):
        candidates.append(Path(os.environ["JH_HOST_ENVIRONMENT"]))
    candidates.extend(
        (
            root_dir.parent / "JaszczurHAL" / ".build" / "windows" / "host-environment.json",
            root_dir.parent.parent
            / "JaszczurHAL"
            / ".build"
            / "windows"
            / "host-environment.json",
        )
    )

    for candidate in candidates:
        path = candidate.expanduser().resolve()
        if path.is_file():
            with path.open("r", encoding="utf-8") as handle:
                return json.load(handle)
    return {}


def resolve_program(
    label: str,
    explicit: Path | None,
    state_value: str | None,
    path_names: tuple[str, ...],
) -> Path:
    candidates: list[Path] = []
    if explicit is not None:
        candidates.append(explicit)
    if state_value:
        candidates.append(Path(state_value))
    for name in path_names:
        discovered = shutil.which(name)
        if discovered:
            candidates.append(Path(discovered))

    for candidate in candidates:
        path = candidate.expanduser().resolve()
        if path.is_file():
            return path
    raise RuntimeError(f"{label} executable was not found")


def resolve_jaszczurhal_root(root_dir: Path, explicit: Path | None) -> Path:
    candidates: list[Path] = []
    if explicit is not None:
        candidates.append(explicit)
    candidates.extend(
        (
            root_dir.parent / "JaszczurHAL",
            root_dir.parent.parent / "JaszczurHAL",
        )
    )

    for candidate in candidates:
        directory = candidate.expanduser().resolve()
        if (directory / "src" / "hal" / "core" / "hal_array.h").is_file():
            return directory
    raise RuntimeError(
        "JaszczurHAL source tree was not found; install Credentials next to "
        "JaszczurHAL or pass --jaszczurhal-root"
    )


def find_bin(candidates: list[Path], compiler: str) -> Path | None:
    for candidate in candidates:
        directory = candidate.expanduser().resolve()
        for name in (f"{compiler}.exe", compiler):
            if (directory / name).is_file():
                return directory
            if (directory / "bin" / name).is_file():
                return directory / "bin"
    return None


def resolve_arm_bin(args: argparse.Namespace, state: dict[str, Any]) -> Path:
    candidates: list[Path] = []
    if args.toolchain_bin is not None:
        candidates.append(args.toolchain_bin)
    legacy = {"rp2040": "RP2040_TOOLCHAIN_BIN", "stm32g474": "STM32_TOOLCHAIN_BIN"}
    for name in (legacy.get(args.target), "CREDENTIALS_TOOLCHAIN_BIN"):
        if name and os.environ.get(name):
            candidates.append(Path(os.environ[name]))
    managed_compiler = state.get("tools", {}).get("gnu-arm")
    if managed_compiler:
        candidates.append(Path(managed_compiler).parent)
    for name in ("arm-none-eabi-gcc", "arm-none-eabi-gcc.exe"):
        compiler = shutil.which(name)
        if compiler:
            candidates.append(Path(compiler).parent)
    directory = find_bin(candidates, "arm-none-eabi-gcc")
    if directory is None:
        raise RuntimeError(f"ARM toolchain was not found for {args.target}")
    return directory


def resolve_riscv_bin(
    args: argparse.Namespace, state: dict[str, Any], registry_path: str | None
) -> Path:
    candidates: list[Path] = []
    if args.toolchain_bin is not None:
        candidates.append(args.toolchain_bin)
    if os.environ.get("CREDENTIALS_TOOLCHAIN_BIN"):
        candidates.append(Path(os.environ["CREDENTIALS_TOOLCHAIN_BIN"]))
    managed_compiler = state.get("tools", {}).get("riscv")
    if managed_compiler:
        candidates.append(Path(managed_compiler).parent)
    if registry_path:
        candidates.append(Path(registry_path))
    directory = find_bin(candidates, "riscv32-unknown-elf-gcc")
    if directory is None:
        raise RuntimeError(f"RISC-V toolchain was not found for {args.target}")
    return directory


def toolchain_environment(toolchain: Path) -> dict[str, str]:
    """Expose the selected compiler and binutils to nested CMake processes."""
    environment = os.environ.copy()
    inherited_path = environment.get("PATH")
    environment["PATH"] = os.pathsep.join(
        [str(toolchain), *([inherited_path] if inherited_path else [])]
    )
    return environment


def target_toolchain(
    args: argparse.Namespace, state: dict[str, Any], jaszczurhal_root: Path
) -> tuple[list[str], dict[str, str] | None]:
    """CMake definitions selecting the firmware toolchain of the target, and
    the environment the build runs in (None: the current one)."""
    scripts = jaszczurhal_root / "scripts"
    sys.path.insert(0, str(scripts))
    from board_registry import tooling_target_registry
    from generate_board_config import load_registry

    registry = tooling_target_registry(jaszczurhal_root)
    if args.target not in registry:
        known = ", ".join(sorted(registry))
        raise RuntimeError(f"unknown target {args.target!r}; known targets: {known}")
    entry = registry[args.target]
    build = load_registry(jaszczurhal_root / "boards")[0][args.target]["build"]
    cache = {
        key: str(value).replace("${jhRoot}", str(jaszczurhal_root))
        for key, value in entry["cache"].items()
    }

    if entry["provider"] == "pico-sdk":
        if "PICO_TOOLCHAIN_PATH" in cache:
            toolchain = resolve_riscv_bin(args, state, cache["PICO_TOOLCHAIN_PATH"])
        else:
            toolchain = resolve_arm_bin(args, state)
        return [
            f"-DPICO_PLATFORM={build['platform']}",
            f"-DPICO_TOOLCHAIN_PATH={toolchain.as_posix()}",
        ], toolchain_environment(toolchain)
    if entry["provider"] == "jh-stm32-baremetal":
        toolchain = resolve_arm_bin(args, state)
        suffix = ".exe" if (toolchain / "arm-none-eabi-gcc.exe").is_file() else ""
        names = {"gcc": "C_COMPILER", "g++": "CXX_COMPILER", "ar": "AR", "ranlib": "RANLIB"}
        # CMake serializes compiler paths into quoted strings. Forward slashes
        # keep Windows backslashes from becoming CMake escape sequences.
        return [
            f"-DCMAKE_TOOLCHAIN_FILE={Path(cache['CMAKE_TOOLCHAIN_FILE']).as_posix()}",
            *(
                f"-DCMAKE_{names[tool]}={(toolchain / f'arm-none-eabi-{tool}{suffix}').as_posix()}"
                for tool in ARM_TOOLS
            ),
        ], toolchain_environment(toolchain)
    if entry["provider"] == "esp-idf":
        from build_esp_idf import exported_environment, prepare_sdk

        idf_dir = jaszczurhal_root / "third_party" / "esp-idf"
        toolchain_file = idf_dir / "tools" / "cmake" / f"toolchain-{build['idfTarget']}.cmake"
        environment = None
        if not args.dry_run:
            # Installs the pinned ESP-IDF and its tools on first use, as an
            # ESP firmware build does.
            environment = exported_environment(prepare_sdk(jaszczurhal_root, ""))
        return [
            f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file.as_posix()}",
            f"-DCREDENTIALS_IDF_TARGET={build['idfTarget']}",
        ], environment
    raise RuntimeError(f"target {args.target!r} has no firmware toolchain")


def main() -> int:
    args = parse_args()
    root_dir = Path(__file__).resolve().parent.parent
    state = load_host_environment(root_dir, args.host_environment)
    tools = state.get("tools", {})

    try:
        cmake = resolve_program(
            "CMake",
            args.cmake,
            tools.get("cmake"),
            ("cmake", "cmake.exe"),
        )
        ninja = resolve_program(
            "Ninja",
            args.ninja,
            tools.get("ninja"),
            ("ninja", "ninja.exe"),
        )
        jaszczurhal_root = resolve_jaszczurhal_root(
            root_dir, args.jaszczurhal_root
        )
        toolchain_args, environment = target_toolchain(args, state, jaszczurhal_root)
    except Exception as error:  # registry and ESP-IDF errors carry their own type
        print(f"error: {error}", file=sys.stderr)
        return 2

    build_dir = (
        args.build_dir.expanduser().resolve()
        if args.build_dir is not None
        else root_dir / "build" / args.target
    )
    # Every target builds into build/<target>/libCredentials.a.
    archive_dir = build_dir
    configure_command = [
        str(cmake),
        "-S",
        str(root_dir),
        "-B",
        str(build_dir),
        "-G",
        "Ninja",
        f"-DCMAKE_MAKE_PROGRAM={ninja.as_posix()}",
        *toolchain_args,
        f"-DCREDENTIALS_TARGET={args.target}",
        f"-DCREDENTIALS_JASZCZURHAL_ROOT={jaszczurhal_root.as_posix()}",
        f"-DCREDENTIALS_ARCHIVE_DIR={archive_dir.as_posix()}",
    ]
    build_command = [
        str(cmake),
        "--build",
        str(build_dir),
        "--target",
        "Credentials",
    ]
    archive = archive_dir / "libCredentials.a"

    if args.dry_run:
        print(
            json.dumps(
                {
                    "configure": configure_command,
                    "build": build_command,
                    "target": args.target,
                    "archive": str(archive),
                },
                indent=2,
            )
        )
        return 0

    for command in (configure_command, build_command):
        completed = subprocess.run(command, check=False, env=environment)
        if completed.returncode != 0:
            return completed.returncode

    if not archive.is_file():
        print(f"error: expected archive was not generated: {archive}", file=sys.stderr)
        return 2
    print(f"Built {archive}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
