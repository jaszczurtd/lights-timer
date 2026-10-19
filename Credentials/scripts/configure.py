#!/usr/bin/env python3
"""Create local Credentials configuration without overwriting existing data."""

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path


CONFIG_FILES = (
    ("CredentialsData", ".h"),
    ("MacHostMapping", ".cpp"),
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--test",
        action="store_true",
        help="Use reserved, non-secret test values instead of editable examples.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root_dir = Path(__file__).resolve().parent.parent
    config_dir = root_dir / "config"
    source_kind = "test" if args.test else "example"

    destinations = [config_dir / f"{name}.local{suffix}" for name, suffix in CONFIG_FILES]
    existing = [path for path in destinations if path.exists()]
    if args.test and existing:
        print(
            "error: refusing to replace existing local configuration with test data:",
            file=sys.stderr,
        )
        for path in existing:
            print(f"  {path}", file=sys.stderr)
        return 2

    created: list[Path] = []
    for name, suffix in CONFIG_FILES:
        source = config_dir / f"{name}.{source_kind}{suffix}"
        destination = config_dir / f"{name}.local{suffix}"
        if destination.exists():
            continue
        shutil.copyfile(source, destination)
        created.append(destination)

    if created:
        print("Created local configuration files:")
        for path in created:
            print(f"  {path}")
    else:
        print("Local configuration already exists; nothing was overwritten.")

    if args.test:
        print("Installed reserved non-secret test configuration.")
    else:
        print("Fill in the local files and set CREDENTIALS_LOCAL_CONFIGURED to 1.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
