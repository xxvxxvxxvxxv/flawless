# Validation — flawless 1.0.0-rc1

## What passed

The final source compiled successfully for **ESP32-S3** using Arduino ESP32 **3.3.0**, its IDF 5.5 toolchain, and the pinned dependencies listed in README.md. This was an actual target compile/link, not just a desktop syntax check.

FQBN:

```text
esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=default_8MB
```

Compiler output:

```text
Sketch uses 1536883 bytes (45%) of program storage space. Maximum is 3342336 bytes.
Global variables use 116824 bytes (35%) of dynamic memory, leaving 210856 bytes for local variables. Maximum is 327680 bytes.

```

The linked ESP32-S3 ELF contains `mqttWorker`, BLE advertisement callback code, `oniKeyboard`, `wsTelemetry`, `mscRead`, `hidReady`, and `usbVendor`. This checks that optional-library discovery did not silently compile those implementations out. It does not prove their peripherals were exercised.

Six C++ test executables passed under C++11 with `-Wall -Wextra -Werror`:

- Detector warmup, bursts, duration, latching and reset.
- Telemetry serialization, protocol validation and sequence accounting.
- Management-frame parsing, addresses and traffic heuristics.
- Beacon information elements, supported rates and bounds.
- FAT12 cluster chains, data layout and out-of-bounds rejection.
- The firmware's macro parser, grammar limits and text handling.

Both browser scripts passed Node syntax checks. `panel.h` matches `web/index.html` exactly. The JSDOM integration suite passed 20 consecutive live polls, offline timeout/reconnect, control identity/caret/draft retention, changing macro slots including empty slot 0, live counters, arming expiry, correct save/start payloads, WebSocket updates, all 16 tabs, and loaded session report retention.

The Hannya and outlined wordmark SVGs were rasterized and visually inspected. The included cover uses those same assets. `SOURCE-SHA256.json` identifies the firmware, web and artwork files used for this release.

## What has not been verified

There was no physical ESP32-S3, nRF24 peer, AMOLED, USB host connection to the device, or over-air receiver in this environment. Therefore boot stability, real memory use under load, pin wiring, display timing, USB enumeration/MSC host caching, Wi-Fi/BLE coexistence, scanner visibility, link-test delivery/RTT, broker connectivity and sleep/wake/purge behavior remain **hardware acceptance checks**.

The global-variable figure above excludes runtime heap allocations, FreeRTOS task stacks, BLE/Wi-Fi buffers and PSRAM rings. It is not a free-memory guarantee while the device is running.

JSDOM verifies preservation of the actual control nodes; it does not open macOS's native select popup. Browser layout was not exercised in a full browser. The GitHub Actions workflow is supplied but has not been run on the user's repository.

Follow the numbered acceptance checks in README.md. Do not describe this release as physically certified or claim that a driver's accepted-transmission counter proves a phone received a beacon.
