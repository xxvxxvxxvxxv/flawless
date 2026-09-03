# Build guide

## Power

Disconnect USB power before changing the wiring. Connect the nRF24L01 to 3.3 V only. Install a 10–100 µF capacitor between VCC and GND near the radio module. The negative stripe of an electrolytic capacitor goes to GND.

## Connections

| Signal | Radio pin | Board pin |
|---|---|---|
| Power | VCC | 3V3 |
| Ground | GND | GND |
| Radio enable | CE | GPIO15 |
| SPI select | CSN | GPIO14 |
| SPI clock | SCK | GPIO12 |
| Controller to radio | MOSI | GPIO11 |
| Radio to controller | MISO | GPIO13 |
| Interrupt | IRQ | Leave open |

The display uses its own QSPI bus. The firmware assigns the nRF24L01 to the ESP32-S3 secondary hardware SPI controller, allowing the radio and AMOLED to operate together.

## Arduino IDE

Install these libraries through Library Manager:

- RF24 by TMRh20
- GFX Library for Arduino by Moon On Our Nation

Open the sketch, choose `ESP32S3 Dev Module`, enable USB CDC on boot, select the correct USB port, and upload.

## First boot

Successful initialization prints:

```text
RF_SENTINEL_READY
DASHBOARD http://192.168.4.1
```

If `NRF24_INIT_FAILED` appears, inspect VCC, GND, MOSI, MISO, SCK, CE, and CSN. If `DISPLAY_INIT_FAILED` appears, confirm the exact board model and selected ESP32-S3 target.
