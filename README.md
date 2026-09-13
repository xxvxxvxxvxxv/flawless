# flawless

![flawless katana banner](branding/cover.png)

A local ESP32-S3 diagnostic instrument with a monochrome katana dashboard, AMOLED display, and optional nRF24L01+ radio. **Version 1.0.0-rc2 is a development build, not a finished hardware release.** The preceding build uploaded successfully and the user reports improved operation; feature acceptance remains incomplete. See [VALIDATION.md](VALIDATION.md) for evidence and remaining hardware checks.

## Current status

- **Reported working:** firmware upload and access to the local dashboard on the preceding build.
- **Current hardware:** ESP32-S3 without an nRF24. Spectrum, waterfall, decoded nRF24 capture and nRF24 link testing require that external radio; the ESP32 Wi-Fi scan is a separate feature.
- **Open issue:** user reports an empty Wi-Fi network list. Investigation is deferred, not marked fixed.
- **Lab Beacons:** still limited to **4 SSIDs**, 10 cycles/s and 60 seconds. A request for 20–30 SSIDs is tracked for a later firmware review; this branding update does not implement it. Driver acceptance is not proof of over-air reception.

See [KNOWN_ISSUES.md](KNOWN_ISSUES.md) for the next engineering pass and [VALIDATION.md](VALIDATION.md) for checks actually performed.

## Start here

1. Extract the project and open **`firmware/flawless/flawless.ino`** in Arduino IDE. All its `.h` files must remain in that same directory. Close older copies of the sketch.
2. Install the exact core and libraries below. Select the listed settings and upload.
3. Connect to **`flawless-xxxxxx`**, password **`observe24`**, then open **http://192.168.4.1**. The dashboard should say **UI · flawless-1.0.0-rc2**. Diagnostics also shows firmware **1.0.0-rc2**; compare both to detect an old flashed sketch or browser page.
4. Check **Diagnostics**: access point and display ready, loop count increasing, PSRAM nonzero, SPI register PASS if the nRF24 is fitted. Missing nRF24 does not disable Wi-Fi/BLE/USB or the web server.
5. If Storage is unavailable on a new or repartitioned device, use **Initialize unavailable storage** in Diagnostics. It asks for confirmation because formatting deletes the existing filesystem. Normal boot never automatically formats it.

The dashboard HTML and artwork are embedded in the firmware: **no separate filesystem uploader is needed for the UI**. Editing `web/index.html` alone does not update the device. Run `python3 tools/embed_panel.py`, then compile and upload the sketch.

## Hardware and exact build settings

The supplied display wiring targets the **Waveshare ESP32-S3 AMOLED 1.91 / 1.91-M configuration used by the original project**, with RM67162, 16 MB flash and 8 MB OPI PSRAM. Check your actual board before using these pins. Other displays/boards need different initialization.

| Component | Tested build version |
|---|---|
| esp32 by Espressif Systems, Boards Manager | **3.3.0** |
| RF24 by TMRh20 | **1.4.11** |
| GFX Library for Arduino by moononournation | **1.6.7** |
| WebSockets by Markus Sattler | **2.7.2** |
| PubSubClient by Nick O'Leary | **2.8.0** |
| BLE, WiFi, LittleFS, USB HID/MSC/Vendor | Included with the ESP32 core above |

| Arduino Tools setting | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| CPU Frequency | 240MHz (WiFi) |
| Flash Mode | QIO 80MHz |
| Flash Size | 16MB (128Mb) |
| PSRAM | OPI PSRAM |
| Partition Scheme | **8M with spiffs (3MB APP/1.5MB SPIFFS)** |
| USB Mode | **USB-OTG (TinyUSB)** |
| USB CDC On Boot | Enabled |
| USB DFU On Boot | Disabled |
| USB Firmware MSC On Boot | Disabled — the sketch registers its own read-only MSC |
| Arduino Runs On / Events Run On | Core 1 / Core 1 |
| Upload Mode | UART0 / Hardware CDC for the manual BOOT/RST procedure below |
| Upload Speed | 115200 for initial troubleshooting; 921600 after reliable uploads |
| Erase All Flash Before Sketch Upload | Disabled |
| Core Debug Level | None |
| JTAG Adapter | Disabled |
| Port | The port currently present for this board; it may change after reset |

The 8M partition layout fits the 16 MB flash and provides both application space and a `spiffs`-labelled data partition that this firmware mounts as **LittleFS**. The remaining flash is unused. **Do not select ESP SR 16M**: that unrelated speech-model layout can request a missing `srmodels.bin`. Do not change partitions on a device whose saved files you need without first exporting them.

If upload reports a missing port or “No serial data received”: close Serial Monitor, hold **BOOT**, briefly press/release **RST/EN**, release **BOOT**, reselect the newly appearing USB port, then upload. After successful verification, release BOOT and press RST/EN once to boot the application. A compilation size report alone does not mean upload succeeded. Do not erase the device to fix a missing USB port.

| nRF24L01+ pin | ESP32-S3 pin |
|---|---|
| VCC / GND | 3V3 / GND |
| CE / CSN | GPIO15 / GPIO14 |
| MOSI / MISO / SCK | GPIO11 / GPIO13 / GPIO12 |
| IRQ | Unconnected |

Display: CS GPIO6, clock GPIO47, QSPI data GPIO18/7/48/5, reset GPIO17. QSPI uses SPI2 and the nRF24 uses SPI3 (Arduino HSPI) in the pinned core. Do not power nRF24 from 5 V. It needs stable 3.3 V power.

The instrument can run from a suitable USB power bank, USB supply, or phone that supplies USB host power. A Mac is only needed for uploading or Mac-hosted functions. **Keyboard macros require a USB data connection to the intended host**; Wi-Fi alone cannot type into a computer. USB MSC and WebUSB also require a data cable.

## Features and their actual scope

| Feature | Implementation and dependency |
|---|---|
| Spectrum / Waterfall | nRF24 RPD threshold-hit sweep over 126 channels, 2400–2525 MHz; 24 device history rows. Not a spectrum analyzer or calibrated dBm measurement. |
| Wi-Fi discovery | ESP32 Wi-Fi scan, up to 24 results, approximately every 30 seconds when no diagnostic owns the radio. |
| Wi-Fi probe observations | ESP32 promiscuous management parser; bounded 64-device table, SSID/RSSI/request count. AP/coexistence channel activity limits coverage. |
| BLE telemetry | ESP32 passive BLE advertisements; 32-device table, name/RSSI/manufacturer bytes. No BLE packet injection or connection interception. |
| Events | 64-entry circular log, serial JSON, browser JSON download. |
| Diagnostics | Register readback, display/AP/PSRAM/heap/loop status, capabilities, storage initialization and runtime errors. Register timing is not RF latency. |
| Link Test | Two compatible nRF24 nodes: explicit replies, sequence accounting, delivery ratios and firmware-scheduled RTT. 1–600 frames at 5 Hz; 130-second maximum. Eight saved reports when storage is mounted. |
| Frames | nRF24-compatible fixed 32-byte payload capture on channel 40 (2440 MHz), 1 Mbps, CRC16, five-byte address. 1,000-entry PSRAM frame ring. Only hardware-accepted packets reach the application. |
| Traffic | Threshold occupancy, sampled burst edges, compatible payload packet counts and microsecond IAT. Heuristic labels, not transmitter identification. |
| Field Track | 128 circular markers with threshold peak and optional user-measured dBm. The device does not derive calibrated RSSI from nRF24 RPD. |
| Anomaly Matrix | High-occupancy and generic payload-signature heuristics over retained observations; no trained threat classifier. |
| WebSocket | Port 81 broadcasts decoded capture frames; HTTP polling supplies other telemetry. |
| UDP | Local AP broadcast activity JSON on UDP port 39001. |
| MQTT | Background worker publishes activity JSON to a configured broker reachable from the AP network; connectivity and success/error counters exposed. |
| Local journal | Opt-in LittleFS logging every 30 seconds while idle; two rotating snapshots, each with the latest 100 observations and current events. |
| USB MSC | Read-only 128 KiB FAT12 snapshot: `EVENTS.JSN`, `FRAMES.CSV`, `README.TXT`. Explicit refresh from Transport; latest 100 frames in this snapshot. |
| WebUSB | Vendor bulk interface plus local browser client; register test and intelligence JSON commands. |
| Host Macros | Composite USB keyboard, six LittleFS script slots, explicit 20-second arm, three-second focus delay, incremental execution and Stop. |
| Lab Beacons | ESP32 core-0 task emits 1–4 synthetic SSIDs, 1–10 cycles/second, 1–60 seconds. Each cycle transmits one frame per SSID; these are advertisements, not usable APs. |
| Emergency Reset | Stops workers, clears telemetry RAM/USB snapshot, deletes known LittleFS telemetry/session files, blanks AMOLED, disables Wi-Fi and sleeps. Preserves macro files. Aborts sleep and reports an error if cleanup cannot be verified. |
| Ghost mode | Opt-in one-second timer light sleep about every ten seconds when idle, with no AP clients or mounted USB host. Radios are paused and restarted. It does not monitor during sleep. |
| RF audio | Browser Web Audio with Chromatic, natural Minor, and Minor Pentatonic mappings; requires a user gesture. |
| Physical UI | Short BOOT press changes AMOLED page. Hold 3–8 seconds and release to arm macros; the next short press runs slot 0. Hold at least 8 seconds and release to request purge. RST/EN restarts firmware. |

ESP32 handles Wi-Fi, BLE, USB, networking and task scheduling. nRF24 remains for its own compatible packet/link diagnostics and threshold survey; ESP32 Wi-Fi is not a drop-in replacement for those nRF24 protocols. Set `FLAWLESS_ENABLE_NRF` to `0` in `build_config.h` for ESP32-only operation. BLE, WebSocket and MQTT have explicit build switches, all enabled by default, so Arduino discovers their real libraries instead of silently omitting features.

RF-Clown's interference/jamming routines are **not included**. This project retains diagnostic transmission and finite lab tests.

## First hardware acceptance checks

Test these one at a time and record the outcome. Passing a compiler or host test cannot verify radio reception, wiring, USB enumeration or power stability.

1. **Boot and basic UI:** leave Diagnostics open for five minutes. Verify loop count/uptime increase, no repeated resets, and nonzero PSRAM. Short-press BOOT through six display pages. Open macro and beacon selectors across several telemetry updates, disconnect/reconnect Wi-Fi, and verify selections/drafts survive. The underlying control nodes are preserved now; polling updates values instead of replacing the page.
2. **Wi-Fi / BLE:** compare Wi-Fi names with your own APs, then advertise from a known BLE device and watch its record. BLE/Wi-Fi scans share the ESP32 radio, so discovery timing varies. Probe requests are not guaranteed to be emitted by a nearby idle phone.
3. **nRF24:** confirm register PASS; without a second transmitter, an empty Frames page is normal. For payload capture, send 32-byte, 1 Mbps, CRC16 packets on channel 40 to address bytes `4F 4E 49 43 31` (`ONIC1`). For Link Test, use two boards running the same protocol: arm the responder first, then start the initiator. Without a peer, delivery should be zero and RTT unavailable.
4. **Macros:** initialize storage if necessary, choose a slot, enter `TEXT flawless diagnostic test`, save, connect USB to the intended host, and check **HID Ready**. Open a blank text editor, arm, click Run, and focus the editor within three seconds. It should type the sentence once. Run loads the saved slot, not an unsaved draft. Test Stop and USB disconnection. Script text is printable ASCII with US keyboard mapping; non-US host layouts may produce different characters.
5. **Lab Beacons:** start with one unique name such as `FLAWLESS-LAB-01`, rate **10**, duration **30000**. Verify state RUNNING, decreasing remaining time, increasing per-SSID accepted counts and `ESP_OK`. Scan from another device **while the test is running**. The test is already over when state reads completed/idle. OS network lists may miss or hide unassociated synthetic beacons: use an independent monitor-capable receiver on the displayed channel to verify reception. A driver-accepted count alone does not prove over-air visibility. Stop should restore monitoring. No clients can join these synthetic names.
6. **Exports / storage:** create events, markers and a link report; download JSON/CSV. Reboot and verify saved reports and macro slots remain. Enable logging, wait over 30 seconds in MONITOR, download the journal. RAM observation/frame rings intentionally start fresh on reboot.
7. **Transport:** for UDP, listen on your AP-connected computer's UDP port 39001. For MQTT, run a broker that listens on that computer's `192.168.4.x` interface, allow it through the computer firewall, and enter that address plus a topic in Transport. There is no station-Wi-Fi provisioning or Internet broker route in this release. MQTT publishes roughly every three seconds and does not provide TLS/authentication settings.
8. **USB:** mount the read-only disk, eject it, click Refresh USB snapshot in Transport, then reconnect USB if the OS caches the old media. Verify JSON parses and CSV quoting. For WebUSB, run `python3 -m http.server 8000 --directory web`, open `http://localhost:8000/webusb.html` in Chrome/Edge, connect and read JSON. This standalone page is included in the repository; it is not served by the device's root dashboard.
9. **Sleep / purge last:** export needed telemetry first. Enable Ghost mode, disconnect clients and use power-only USB; verify wake and reconnection. For purge, ensure storage is mounted, type `PURGE` when asked, and verify the display goes black and the AP disappears. Use RST/EN or BOOT wake to restart; saved telemetry/reports should be absent, macro slots retained. Deleting flash files is logical deletion, **not forensic secure erasure**.

If a step fails, capture Diagnostics JSON from `/diagnostics` and `/data`, plus Serial Monitor at **115200 baud**. Serial prints automatically; `d` requests register readback. Early startup messages may be missed while USB re-enumerates. Note the failing feature and actual error rather than assuming a populated dashboard proves it works.

## Repository checks and reproducible build

```sh
python3 tools/embed_panel.py
python3 tools/check.py
npm ci
npm test
```

`tools/check.py` runs the portable C++ tests, JavaScript syntax checks, and exact embedded-panel synchronization check. `npm test` uses JSDOM to exercise live polling, timeout/reconnect, all dashboard tabs and draft retention; it does not emulate the native OS dropdown popup or ESP32 peripherals.

```sh
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.0 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install 'RF24@1.4.11' 'GFX Library for Arduino@1.6.7' 'WebSockets@2.7.2' 'PubSubClient@2.8.0'
arduino-cli compile --fqbn 'esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=default_8MB' firmware/flawless
```

The command records build settings explicitly, so they do not depend on whichever IDE window you opened last. GitHub Actions repeats the host tests and pinned target build.

## Upload to GitHub

Extract this ZIP. The repository contents are the files **inside `flawless/`**. In GitHub Desktop, clone your existing repository, copy these contents into it, review the Changes tab, commit, and push. Remove any obsolete `firmware/oni_sentinel` or `firmware/rf_sentinel_s3` directory from that repository so there is one authoritative sketch. For a new repository, add this extracted folder in GitHub Desktop and Publish repository. Do not upload only a firmware binary or the ZIP as the source tree.

The original `oni` C++ namespace, nRF24 protocol signature, capture address and `X-Oni-Token` header remain for compatibility; product branding, AP name, sketch directory and download names are `flawless`.

## License and access model

Code: [MIT](LICENSE). The approved generated katana artwork is included as `branding/katana.png` and embedded directly in the dashboard. The banner uses the same unmodified artwork. Earlier vector artwork is retained as source history; its Metal Mania wordmark retains its [SIL Open Font License and attribution](branding/FONT-LICENSE.txt). Active branding works without Internet access.

The dashboard is local HTTP on a password-protected AP. Its per-boot control token protects control requests from accidental/replayed UI actions; any connected client can obtain it. It is not a user-account authentication system. Change the default AP password in `setup()` for your deployment. Use observation and transmission features with your own equipment in an authorized test area.
