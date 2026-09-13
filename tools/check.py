#!/usr/bin/env python3
"""Portable source checks; does not emulate hardware."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="flawless-check-") as tmp:
    for src in sorted((root / "tests").glob("*_test.cpp")):
        binary = Path(tmp) / src.stem
        subprocess.run(["g++", "-std=c++11", "-Wall", "-Wextra", "-Werror", str(src), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
        print("PASS", src.name, flush=True)
    for html in sorted((root / "web").glob("*.html")):
        script = Path(tmp) / (html.stem + ".js")
        script.write_text("\n".join(re.findall(r"<script[^>]*>(.*?)</script>", html.read_text(), flags=re.S)))
        subprocess.run(["node", "--check", str(script)], check=True)
        print("PASS JavaScript syntax:", html.name, flush=True)
html = (root / "web/index.html").read_text()
expected = '#pragma once\n#include <Arduino.h>\nstatic const char PANEL[] PROGMEM = R"ONIPANEL(' + html + ')ONIPANEL";\n'
assert (root / "firmware/flawless/panel.h").read_text() == expected, "Run python3 tools/embed_panel.py"
print("PASS embedded panel matches web/index.html")
