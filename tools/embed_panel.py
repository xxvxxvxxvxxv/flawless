from pathlib import Path
root = Path(__file__).resolve().parents[1]
html = (root / 'web/index.html').read_text()
assert ')ONIPANEL"' not in html
(root / 'firmware/flawless/panel.h').write_text('#pragma once\n#include <Arduino.h>\nstatic const char PANEL[] PROGMEM = R"ONIPANEL(' + html + ')ONIPANEL";\n')
