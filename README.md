<p align="center">
  <img src="branding/cover.png" alt="flawless — ESP32-S3 field diagnostics" width="100%">
</p>

<p align="center">
  <strong>Portable diagnostics. Local control.</strong><br>
  ESP32-S3 · Wi-Fi · BLE · USB · Optional nRF24L01+
</p>

---

**flawless** is an open-source handheld diagnostic instrument built around the ESP32-S3. It combines an AMOLED display with a local browser dashboard for wireless observations, USB automation, telemetry, and hardware diagnostics.

The device hosts its own Wi-Fi network, so the dashboard works without an internet connection. An optional nRF24L01+ module adds RF activity monitoring and compatible packet diagnostics. The black-and-white interface takes its visual direction from Japanese katana artwork.

## Capabilities

| Area | Functionality |
| --- | --- |
| **Local dashboard** | Live telemetry, event history, hardware status, and controls accessible over the device's Wi-Fi network. |
| **Wi-Fi & BLE** | Nearby Wi-Fi discovery, Wi-Fi probe observations, and passive BLE advertisement records. |
| **USB automation** | Six stored keyboard-macro slots, explicit arming, delayed execution, and cancellation. |
| **RF monitoring** | Spectrum-style activity and waterfall views, compatible payload capture, and two-node link tests using an optional nRF24L01+. |
| **Lab Beacons** | Synthetic beacon advertisements with configurable names, rate, and duration. |
| **Data & transport** | JSON/CSV exports, local logging, WebSocket capture streaming, UDP, and MQTT publishing. |
| **USB access** | Read-only telemetry snapshot and a WebUSB diagnostic client. |
| **Device controls** | AMOLED navigation, optional light sleep, and telemetry purge followed by deep sleep. |

## Project status

**1.0.0-rc2 · Active development**

The firmware compiles for ESP32-S3 and passes the included host tests. Hardware acceptance is ongoing. Known limitations include:

- Wi-Fi discovery can show an empty list; investigation is open.
- Spectrum, waterfall, nRF24 capture, and link tests require the external nRF24 module. ESP32 Wi-Fi scanning does not replace those measurements.
- Lab Beacons currently supports **four SSIDs**, up to **10 cycles per second**, for up to **60 seconds**. These are beacon advertisements, not joinable access points; receiver visibility still needs verification.
- nRF24 activity readings are threshold-hit percentages, not calibrated signal power.

[Validation results](VALIDATION.md) · [Known issues](KNOWN_ISSUES.md) · [Changelog](CHANGELOG.md)

## Hardware

The current configuration targets the **Waveshare ESP32-S3 AMOLED 1.91 / 1.91-M**, with an RM67162 display, **16 MB flash**, and **8 MB OPI PSRAM**.

| Optional nRF24L01+ connection | ESP32-S3 |
| --- | --- |
| VCC / GND | 3.3 V / GND |
| CE / CSN | GPIO15 / GPIO14 |
| MOSI / MISO / SCK | GPIO11 / GPIO13 / GPIO12 |

Wi-Fi, BLE, and USB use the ESP32-S3 directly. Set `FLAWLESS_ENABLE_NRF` to `0` in `build_config.h` for operation without the external radio. USB keyboard, storage, and WebUSB functions require a data connection to the host; standalone operation can use a suitable USB power supply.

## Getting started

1. Open `firmware/flawless/flawless.ino` in Arduino IDE.
2. Install the dependencies and select the board settings below.
3. Upload the firmware, then restart the board with BOOT released.
4. Join **`flawless-xxxxxx`** using the default password **`observe24`**.
5. Open **http://192.168.4.1** and check **Diagnostics** for hardware and storage status.

The dashboard is embedded in the firmware. No separate filesystem upload is required. Storage initialization is available from Diagnostics when needed; formatting deletes existing filesystem contents.

<details>
<summary><strong>Dependencies and Arduino settings</strong></summary>

| Dependency | Build version |
| --- | --- |
| ESP32 by Espressif Systems | 3.3.0 |
| RF24 by TMRh20 | 1.4.11 |
| GFX Library for Arduino | 1.6.7 |
| WebSockets by Markus Sattler | 2.7.2 |
| PubSubClient by Nick O'Leary | 2.8.0 |

Wi-Fi, BLE, LittleFS, and USB libraries are supplied by the ESP32 core. The RF24 library is required by the current source even when the physical module is omitted.

| Arduino setting | Value |
| --- | --- |
| Board | ESP32S3 Dev Module |
| CPU Frequency | 240 MHz |
| Flash Mode / Size | QIO 80 MHz / 16 MB |
| PSRAM | OPI PSRAM |
| Partition Scheme | 8M with spiffs (3MB APP/1.5MB SPIFFS) |
| USB Mode | USB-OTG (TinyUSB) |
| USB CDC On Boot | Enabled |
| USB DFU / Firmware MSC On Boot | Disabled |
| Arduino / Events Run On | Core 1 |
| Upload Mode | UART0 / Hardware CDC |
| Upload Speed | 115200; 921600 with a reliable connection |
| Erase All Flash Before Upload | Disabled |
| Core Debug Level / JTAG Adapter | None / Disabled |

The partition layout uses part of the board's 16 MB flash. Its data partition is mounted as LittleFS. The ESP SR partition layout is not used.

If automatic upload fails, hold BOOT, press and release RST/EN, then release BOOT. Select the newly enumerated port and upload. Press RST/EN afterward with BOOT released.

</details>

## Development

| Directory | Contents |
| --- | --- |
| `firmware/flawless/` | Arduino sketch and firmware modules |
| `web/` | Dashboard source and WebUSB client |
| `branding/` | Logo and banner assets |
| `tests/` | Portable C++ and dashboard regression tests |
| `tools/` | Dashboard embedding and validation scripts |

After editing the dashboard, regenerate its embedded header:

```sh
python3 tools/embed_panel.py
```

Run the repository checks:

```sh
python3 tools/check.py
npm ci
npm test
```

The GitHub Actions workflow defines host checks and an ESP32-S3 build with pinned dependencies. Host tests cover parsers, telemetry, beacon layout, storage snapshots, and dashboard control retention. Physical radio, USB, display, and power behavior require testing on hardware.

## License

Source code is available under the [MIT License](LICENSE). Font attribution for the included legacy wordmark is retained in [branding/FONT-LICENSE.txt](branding/FONT-LICENSE.txt).
