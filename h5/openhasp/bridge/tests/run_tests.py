#!/usr/bin/env python3
"""Validate layout/mapping agreement, then compile and run the actual bridge core."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
subprocess.run([sys.executable, str(ROOT / "generate_mapping.py"), "--check"], check=True)
mapping = json.loads((ROOT.parent / "mapping.json").read_text())
objects = {}
for line in (ROOT.parent / "pages.jsonl").read_text().splitlines():
    if not line.strip():
        continue
    obj = json.loads(line)
    key = (obj["page"], obj["id"])
    assert key not in objects, f"Duplicate layout object: {key}"
    objects[key] = obj
for row in mapping["components"]:
    obj = objects[(row["page"], row["id"])]
    assert row["openhasp"] == f'p{row["page"]}b{row["id"]}'
    if row["action"] and row["nextion_page"] is not None:
        assert obj["tag"] == {"nextion_page": row["nextion_page"],
                              "nextion_id": row["nextion_component_id"]}
        assert obj["toggle"] is False and obj["click"] is True
    if row["live_text"]:
        assert "text" in obj
print("PASS: generated mapping and layout tags agree", flush=True)
with tempfile.TemporaryDirectory(prefix="nanoels-bridge-") as temporary:
    binary = str(Path(temporary) / "test_bridge")
    command = shlex.split(os.environ.get("CXX", "c++"))
    command += ["-std=c++11", "-Wall", "-Wextra", "-Werror", "-pedantic"]
    command += shlex.split(os.environ.get("CXXFLAGS", ""))
    subprocess.run(command + ["-I", str(ROOT), str(ROOT / "tests/test_bridge.cpp"),
                               "-o", binary], check=True)
    subprocess.run([binary], check=True)
