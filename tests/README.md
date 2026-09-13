# Checks

Run `python3 tools/check.py` and `npm ci && npm test` from the repository root.

- `detector_test.cpp`: threshold warmup, burst duration, latching and reset.
- `telemetry_test.cpp`: binary protocol validation, sequence accounting, gaps, duplicates and bounds.
- `intelligence_test.cpp`: bounded management-frame parsing, addresses and traffic heuristics.
- `lab_beacon_test.cpp`: frame information elements, channel/rate/size validation and timestamps.
- `fat12_test.cpp`: FAT12 cluster chains, directory/data layout, size rejection and buffer bounds.
- `macro_test.cpp`: actual portable script parser, valid actions, ASCII restriction, action and text limits, CRLF and trailing spaces.
- `dashboard_test.cjs`: JSDOM integration tests for polling, offline timeout/reconnect, live status, control-node identity across all 16 tabs, drafts, caret, script payload, WebSocket updates and saved reports.

Host tests do not emulate ESP32 peripherals, native OS popup rendering, radio delivery or power behavior. Follow the physical acceptance checklist in README.md before calling hardware features verified.
