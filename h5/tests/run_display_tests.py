#!/usr/bin/env python3
"""Host-test wire formats and the actual H5 adapter functions. Python 3 + C++11."""
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "h5.ino").read_text()
mapping = json.loads((ROOT / "openhasp/mapping.json").read_text())["components"]


def section(start, end):
    return source[source.index(start):source.index(end, source.index(start))]


def compiler_is_clang(command):
    version = subprocess.run(command + ["--version"], check=True, text=True,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return "clang" in version.stdout.lower()


with tempfile.TemporaryDirectory(prefix="h5-display-") as temporary:
    tmp = Path(temporary)
    functions = "\n".join(re.findall(r"^#define B_.*$", source, re.M)) + "\n"
    functions += section("byte lastScreenPageId", "volatile bool firmwareUploadActive")
    functions += section("void writeScreenBytes(", "// Returns number of letters printed.")
    functions += section("const byte HEX_TO_KEYCODE", "void setModeFromUi(")
    functions += section("void beep() {", "void taskDisplay(")
    (tmp / "h5_display_functions.inc").write_text(functions)
    cases = ["struct TextCase { const char* name; int page, id; };", "const TextCase textCases[] = {"]
    for row in mapping:
        if row["live_text"]:
            cases.append('{"%s", %d, %d},' % (row["name"], row["page"], row["id"]))
    cases += ["};", "struct TouchCase { const char* topic; byte page, id; int action; };",
              "const TouchCase touchCases[] = {"]
    for row in mapping:
        if row["action"] and row["nextion_page"] is not None:
            cases.append('{"%s", %d, %d, %s},' % (row["openhasp"], row["nextion_page"],
                         row["nextion_component_id"], row["action"]))
    cases += ["};"]
    (tmp / "display_test_cases.inc").write_text("\n".join(cases))
    for name in ("display_protocol_test", "display_integration_test"):
        command = shlex.split(os.environ.get("CXX", "c++"))
        command += ["-std=c++11", "-Wall", "-Wextra", "-Werror"]
        # The sketch's existing dense [index] array initializer is accepted by
        # Arduino's GNU C++ build. Apple Clang reports it as a C99 extension,
        # so suppress only that known upstream diagnostic in the host harness.
        if compiler_is_clang(command):
            command += ["-Wno-c99-designator"]
        if name == "display_protocol_test":
            command += ["-pedantic"]
        command += shlex.split(os.environ.get("CXXFLAGS", ""))
        binary = str(tmp / name)
        subprocess.run(command + ["-I", str(ROOT), "-I", str(tmp),
                                  str(ROOT / "tests" / (name + ".cpp")), "-o", binary], check=True)
        subprocess.run([binary], check=True)
