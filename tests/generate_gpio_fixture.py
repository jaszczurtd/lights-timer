"""Alter electrical settings while retaining the production wiring."""

import json
from pathlib import Path
import sys

source, output = map(Path, sys.argv[1:])
tree = json.loads(source.read_text())
for index, pull in enumerate(("down", "none", "up", "down"), start=1):
    tree["nodes"][f"button{index}"]["properties"] = {
        "pull": pull, "activeLow": index % 2 == 0}
    tree["nodes"][f"relay{index}"]["properties"] = {"activeLow": index % 2 == 1}
output.mkdir(parents=True, exist_ok=True)
(output / "device_tree.json").write_text(json.dumps(tree))
