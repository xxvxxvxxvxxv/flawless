# Test plan

## 1. Initialization

- Upload the firmware.
- Open Serial Monitor at 115200 baud.
- Confirm `RF_SENTINEL_READY`.
- Confirm the spectrum page appears on the AMOLED.

## 2. Display controls

- Press the built-in BOOT button once and confirm the waterfall page appears.
- Press BOOT again and confirm the Wi-Fi context page appears.
- Press BOOT again and confirm the spectrum page returns.

## 3. Activity response

- Record the baseline with nearby 2.4 GHz devices idle.
- Start a download through a nearby 2.4 GHz Wi-Fi access point.
- Confirm increased occupancy across the access point channel width.
- Stop the download and confirm activity falls toward baseline.

## 4. Dashboard

- Connect to `RF-Sentinel-S3` using password `observe24`.
- Open `http://192.168.4.1`.
- Confirm the graph and frame counter update.
- Confirm the displayed peak corresponds approximately to the AMOLED view.

## 5. Stability

- Run the device for 30 minutes.
- Confirm there are no resets or frozen frames.
- Touch the regulator and nRF24L01 carefully; neither should become abnormally hot.

## Evidence for the repository

Capture:

- A clean top-down hardware photograph.
- A close-up of the spectrum page.
- A close-up of the waterfall page.
- A desktop or phone screenshot of the dashboard.
- A short video showing activity rise during a normal Wi-Fi transfer.

Do not claim calibrated RSSI, packet decoding, Bluetooth tracking, or professional spectrum-analyzer accuracy.
