# Changes in flawless 1.0.0-rc2

- Applied the approved katana artwork to the dashboard and GitHub banner.
- Updated responsive header, device description and development-build identification.
- Recorded empty Wi-Fi discovery, absent nRF24 spectrum and requested beacon capacity in KNOWN_ISSUES.md.
- Kept feature limits and diagnostic runtime behavior unchanged for this presentation update.
- Updated validation evidence to distinguish host checks from user-reported hardware observations.

# Changes in flawless 1.0.0-rc1

This is a repair and rebrand of the uploaded Oni Sentinel source, not a claim of completed physical certification.

- Replaced periodic whole-panel rebuilds with mounted controls and live status updates. Dropdowns, textarea selection/caret, beacon inputs and session reports survive polling, timeouts and WebSocket traffic. Macro drafts are isolated by slot; saved scripts load from the firmware.
- Replaced optional-header guessing with explicit BLE/WebSocket/MQTT feature switches so Arduino library discovery includes the actual implementations. USB HID availability and host readiness are separate status fields.
- Moved BLE and Wi-Fi callback observations through a bounded FreeRTOS queue. The main loop owns String tables and circular storage. Buffers allocate in PSRAM with failure reporting, instead of exhausting static DRAM.
- Corrected microsecond timestamps used by IAT, frame CSV channel reporting/quoting, field-marker wraparound and peak-channel selection. Binary manufacturer data retains zero bytes.
- Added diagnostic mode exclusions, asynchronous BLE scanning, incremental keyboard typing with focus delay and cancellation, and observation pause/resume around active tests.
- Corrected beacon supported-rate information elements. Every cycle visits every configured SSID. Worker startup uses a dedicated queue, separate from cancellation notifications. Stop/completion returns control to monitoring. Exposed driver errors and per-SSID acceptance; retained finite lab limits.
- Moved broker connection and MQTT publishing to a separate core-0 worker with a bounded queue and connection timeouts. Broker targets can be AP-connected computers; no nonexistent station connection is required.
- Rebuilt USB snapshot as valid FAT12 with correct cluster chains, directory records and read bounds. Added explicit refresh, read-only handling and purge invalidation. Added a WebUSB browser client and chunked firmware responses.
- Added opt-in rotating LittleFS telemetry journal and explicit storage initialization. Macro writes use a temporary file and rename. Normal boot does not format storage.
- Purge now stops producers and waits for background workers before clearing buffers/files. Failed cleanup leaves the board awake and reports the error. Light sleep pauses/restores radios only while idle and disconnected.
- Added firmware/version, startup, reset, display, AP, loop, PSRAM, queue-drop, storage, USB and transport status so failures are visible.
- Renamed product/AP/sketch to flawless. Replaced the header logo with an angular horned Hannya and a locally embedded horror wordmark. Added a GitHub cover, exact build settings, feature acceptance checklist and pinned CI build.

RF-Clown interference routines are not merged. nRF24 remains only where its radio/protocol is required; ESP32 owns Wi-Fi, BLE, USB and networking.
