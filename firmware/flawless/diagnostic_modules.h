#pragma once

/*
   Controlled diagnostic extensions.

   The HID path is deliberately a small, allow-listed macro runner. It does
   not accept arbitrary HID reports or execute commands on its own. The Wi-Fi
   path emits only locally-generated lab beacons with a random local BSSID,
   strict duration/rate limits, and no association/deauthentication logic.
*/

#include <LittleFS.h>
#include <ctype.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <atomic>
#include "lab_beacon.h"
#include <soc/soc_caps.h>
#include <sdkconfig.h>
#include <esp_system.h>

#if FLAWLESS_USB_OTG && SOC_USB_OTG_SUPPORTED && defined(CONFIG_TINYUSB_HID_ENABLED) &&            \
    CONFIG_TINYUSB_HID_ENABLED
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <tusb.h>
#define ONI_HAS_HID 1
#else
#define ONI_HAS_HID 0
#endif

constexpr uint8_t ONI_MACRO_CAPACITY = 6;
constexpr uint16_t ONI_MACRO_MAX_SCRIPT = 2048;
constexpr uint8_t ONI_MACRO_MAX_ACTIONS = 96;
constexpr uint32_t ONI_MACRO_ARM_MS = 20000;
constexpr uint32_t ONI_BUTTON_ARM_MS = 3000;
constexpr uint32_t ONI_BUTTON_RESET_MS = 8000;
constexpr uint8_t ONI_LAB_MAX_SSIDS = 4;
constexpr uint8_t ONI_LAB_MAX_RATE_HZ = 10;
constexpr uint32_t ONI_LAB_MAX_DURATION_MS = 60000;

#include "macro_script.h"

String diagnosticStatusJson();
bool diagnosticLabActive();
void diagnosticSetup();
void diagnosticTick();
bool diagnosticButtonRelease(uint32_t heldMs);

bool macroArmed = false;
uint64_t macroArmedUntil = 0;
bool macroRunning = false;
uint8_t macroRunningId = 0;
uint8_t macroActionIndex = 0;
uint64_t macroNextAt = 0;
uint32_t macroRuns = 0;
uint32_t macroRejected = 0;
OniMacroAction macroActions[ONI_MACRO_MAX_ACTIONS];
uint8_t macroActionCount = 0;

bool labActive = false;
uint8_t labRateHz = 10;
uint8_t labSsidCount = 0;
uint32_t labDurationMs = 0;
uint64_t labUntil = 0;
std::atomic<uint32_t> labSent{0}, labErrors{0};
std::atomic<uint32_t> labPerSsid[ONI_LAB_MAX_SSIDS];
std::atomic<int> labLastError{ESP_OK};
std::atomic<bool> labStopRequested{false}, labDone{false};
uint8_t labBssids[ONI_LAB_MAX_SSIDS][6] = {};
uint8_t labChannel = 1;
const char *labEndReason = "idle";
String labSsids[ONI_LAB_MAX_SSIDS];
TaskHandle_t labTaskHandle = nullptr;
QueueHandle_t labStartQueue = nullptr;

bool resetRequested = false;
bool resetInProgress = false;
const char *resetReason = "web";

#if ONI_HAS_HID
USBHIDKeyboard oniKeyboard;
bool oniUsbHidReady = false;
#endif

bool hidReady() {
#if ONI_HAS_HID
  return oniUsbHidReady && tud_hid_ready();
#else
  return false;
#endif
}
bool usbHostMounted() {
#if ONI_HAS_HID
  return tud_mounted();
#else
  return false;
#endif
}
bool diagnosticBusy() {
  return testing || captureEnabled || labActive || macroRunning || resetInProgress ||
         resetRequested;
}
const char *runtimeMode() {
  if (resetInProgress)
    return "PURGE";
  if (testing)
    return "LINK TEST";
  if (captureEnabled)
    return "CAPTURE";
  if (labActive)
    return "LAB BEACONS";
  if (macroRunning)
    return "MACRO";
  return "MONITOR";
}
String macroError = "";
size_t macroTextIndex = 0;
void stopMacro(const char *reason) {
#if ONI_HAS_HID
  if (oniUsbHidReady)
    oniKeyboard.releaseAll();
#endif
  macroRunning = false;
  macroArmed = false;
  macroError = reason;
}

bool macroIdValid(const String &id) {
  return id.length() == 1 && id[0] >= '0' && id[0] < ('0' + ONI_MACRO_CAPACITY);
}

String macroPath(uint8_t id) { return "/macro" + String(id) + ".txt"; }

bool parseUnsigned(const String &value, uint32_t minimum, uint32_t maximum, uint32_t &out) {
  if (!value.length() || value.length() > 6)
    return false;
  for (size_t i = 0; i < value.length(); ++i)
    if (value[i] < '0' || value[i] > '9')
      return false;
  uint32_t parsed = value.toInt();
  if (parsed < minimum || parsed > maximum)
    return false;
  out = parsed;
  return true;
}

bool parseMacroScript(const String &script) {
  return flawless::parseMacro(script.c_str(), script.length(), macroActions, ONI_MACRO_MAX_ACTIONS,
                              macroActionCount);
}

String macroListJson() {
  if (!fsOK)
    return "[]";
  String result = "[";
  bool comma = false;
  for (uint8_t i = 0; i < ONI_MACRO_CAPACITY; ++i) {
    String path = macroPath(i);
    if (!LittleFS.exists(path))
      continue;
    File f = LittleFS.open(path, "r");
    size_t size = f ? f.size() : 0;
    if (f)
      f.close();
    if (comma)
      result += ',';
    result += "{\"id\":" + String(i) + ",\"size\":" + String(size) + "}";
    comma = true;
  }
  return result + "]";
}

bool loadMacro(uint8_t id) {
  File file = LittleFS.open(macroPath(id), "r");
  if (!file || file.size() == 0 || file.size() > ONI_MACRO_MAX_SCRIPT) {
    if (file)
      file.close();
    return false;
  }
  String script = file.readString();
  file.close();
  return parseMacroScript(script);
}

bool startMacro(uint8_t id) {
  if (!macroArmed || nowMs() >= macroArmedUntil || macroRunning || testing || captureEnabled ||
      labActive || resetInProgress) {
    ++macroRejected;
    return false;
  }
#if ONI_HAS_HID
  if (!hidReady() || !loadMacro(id)) {
    ++macroRejected;
    return false;
  }
  macroRunning = true;
  macroRunningId = id;
  macroActionIndex = 0;
  macroNextAt = nowMs() + 3000; // Time to focus the intended host editor.
  macroTextIndex = 0;
  macroError = "";
  pauseObservations();
  macroArmed = false; // A run consumes the arm, preventing accidental replays.
  ++macroRuns;
  logEvent("MACRO_START", id);
  return true;
#else
  (void)id;
  ++macroRejected;
  return false;
#endif
}

void macroTick() {
  if (!macroRunning || nowMs() < macroNextAt)
    return;
#if ONI_HAS_HID
  if (!tud_mounted() || tud_suspended()) {
    stopMacro("USB disconnected or suspended");
    resumeObservations();
    return;
  }
  if (!hidReady())
    return;
  if (macroActionIndex >= macroActionCount) {
    stopMacro("");
    resumeObservations();
    logEvent("MACRO_DONE", macroRunningId);
    return;
  }
  const OniMacroAction &action = macroActions[macroActionIndex];
  size_t sent = 1;
  switch (action.type) {
  case ONI_MACRO_TEXT:
    if (action.text[macroTextIndex])
      sent = oniKeyboard.write(uint8_t(action.text[macroTextIndex++]));
    if (action.text[macroTextIndex]) {
      macroNextAt = nowMs() + 10;
      break;
    }
    macroTextIndex = 0;
    ++macroActionIndex;
    macroNextAt = nowMs() + 25;
    break;
  case ONI_MACRO_ENTER:
    sent = oniKeyboard.write(KEY_RETURN);
    ++macroActionIndex;
    break;
  case ONI_MACRO_TAB:
    sent = oniKeyboard.write(KEY_TAB);
    ++macroActionIndex;
    break;
  case ONI_MACRO_ESC:
    sent = oniKeyboard.write(KEY_ESC);
    ++macroActionIndex;
    break;
  case ONI_MACRO_BACKSPACE:
    sent = oniKeyboard.write(KEY_BACKSPACE);
    ++macroActionIndex;
    break;
  case ONI_MACRO_CTRL:
    sent = oniKeyboard.press(KEY_LEFT_CTRL);
    sent = sent && oniKeyboard.write(action.key);
    oniKeyboard.releaseAll();
    ++macroActionIndex;
    break;
  case ONI_MACRO_DELAY:
    ++macroActionIndex;
    break;
  }
  if (!sent) {
    stopMacro("USB report failed or unsupported character");
    resumeObservations();
    return;
  }
  if (action.type != ONI_MACRO_TEXT)
    macroNextAt = nowMs() + (action.type == ONI_MACRO_DELAY ? action.delayMs : 25);
#endif
}

void armDiagnostics() {
  macroArmed = true;
  macroArmedUntil = nowMs() + ONI_MACRO_ARM_MS;
  logEvent("DIAG_ARMED");
}

bool diagnosticButtonRelease(uint32_t heldMs) {
  if (heldMs >= ONI_BUTTON_RESET_MS) {
    resetReason = "button";
    resetRequested = true;
    return true;
  }
  if (heldMs >= ONI_BUTTON_ARM_MS) {
    if (!diagnosticBusy() && hidReady())
      armDiagnostics();
    return true;
  }
  if (macroArmed && macroArmedUntil > nowMs()) {
    startMacro(0);
    return true;
  }
  return false;
}

// The worker persists across tests. Configuration is immutable until labDone.
void labBeaconTask(void *) {
  for (;;) {
    uint8_t command;
    xQueueReceive(labStartQueue, &command, portMAX_DELAY);
    // Start requests have their own queue. A late stop notification must never
    // be mistaken for a new test after completion.
    ulTaskNotifyTake(pdTRUE, 0);
    const uint32_t interval = oni::beaconPeriodMs(labRateHz);
    uint64_t nextCycle = nowMs();
    while (!labStopRequested.load() && nowMs() < labUntil) {
      for (uint8_t i = 0; i < labSsidCount; ++i) {
        if (labStopRequested.load() || nowMs() >= labUntil)
          break;
        uint8_t frame[128];
        const size_t length =
            oni::buildBeacon(frame, sizeof(frame), labSsids[i].c_str(), labBssids[i], labChannel,
                             interval, uint64_t(esp_timer_get_time()));
        // System sequence numbers are required when the AP has connected clients.
        const esp_err_t result =
            length ? esp_wifi_80211_tx(WIFI_IF_AP, frame, length, true) : ESP_ERR_INVALID_ARG;
        if (result == ESP_OK) {
          ++labSent;
          ++labPerSsid[i];
        } else {
          ++labErrors;
          labLastError.store(result);
        }
      }
      nextCycle += interval;
      const uint64_t now = nowMs();
      // Skip missed periods instead of transmitting a catch-up burst.
      if (nextCycle <= now)
        nextCycle = now + interval;
      const uint64_t wake = nextCycle < labUntil ? nextCycle : labUntil;
      if (wake > now) {
        TickType_t ticks = pdMS_TO_TICKS(uint32_t(wake - now));
        if (!ticks)
          ticks = 1;
        ulTaskNotifyTake(pdTRUE, ticks);
      }
    }
    labDone.store(true); // Publish completion; core 1 restores monitoring.
  }
}

bool startLab(const String &ssids, uint32_t rate, uint32_t duration) {
  if (diagnosticBusy() || !apOK || !ssids.length() || ssids.length() > ONI_LAB_MAX_SSIDS * 33 - 1 ||
      rate < 1 || rate > ONI_LAB_MAX_RATE_HZ || duration < 1000 ||
      duration > ONI_LAB_MAX_DURATION_MS || WiFi.scanComplete() == WIFI_SCAN_RUNNING)
    return false;
  String candidates[ONI_LAB_MAX_SSIDS];
  uint8_t count = 0;
  int start = 0;
  while (start <= int(ssids.length())) {
    if (count == ONI_LAB_MAX_SSIDS)
      return false;
    int end = ssids.indexOf(',', start);
    if (end < 0)
      end = ssids.length();
    String candidate = ssids.substring(start, end);
    candidate.trim();
    if (!candidate.length() || candidate.length() > 32)
      return false;
    for (size_t i = 0; i < candidate.length(); ++i)
      if (uint8_t(candidate[i]) < 32 || uint8_t(candidate[i]) > 126)
        return false;
    for (uint8_t i = 0; i < count; ++i)
      if (candidates[i] == candidate)
        return false;
    candidates[count++] = candidate;
    start = end + 1;
  }
  wifi_second_chan_t secondary;
  if (esp_wifi_get_channel(&labChannel, &secondary) != ESP_OK || !labChannel)
    return false;
  if (!labStartQueue)
    labStartQueue = xQueueCreate(1, sizeof(uint8_t));
  if (!labStartQueue)
    return false;
  if (!labTaskHandle && xTaskCreatePinnedToCore(labBeaconTask, "oni-lab", 4096, nullptr, 1,
                                                &labTaskHandle, 0) != pdPASS)
    return false;
  labSsidCount = count;
  uint8_t base[6];
  esp_fill_random(base, sizeof(base));
  base[0] = (base[0] | 2) & 0xfe;
  for (uint8_t i = 0; i < count; ++i) {
    labSsids[i] = candidates[i];
    memcpy(labBssids[i], base, 6);
    labBssids[i][5] ^= i;
    labPerSsid[i].store(0);
  }
  labRateHz = uint8_t(rate);
  labDurationMs = duration;
  labUntil = nowMs() + duration;
  labSent.store(0);
  labErrors.store(0);
  labLastError.store(ESP_OK);
  labStopRequested.store(false);
  labDone.store(false);
  labEndReason = "running";
  pauseObservations();
  labActive = true;
  const uint8_t command = 1;
  if (xQueueSend(labStartQueue, &command, 0) != pdTRUE) {
    labActive = false;
    labEndReason = "start failed";
    resumeObservations();
    return false;
  }
  logEvent("LAB_BEACON_START", labRateHz, labSsidCount);
  return true;
}

void stopLab() {
  if (labActive && !labDone.load() && !labStopRequested.exchange(true))
    xTaskNotifyGive(labTaskHandle);
}

void finishLabIfDone() {
  if (!labActive || !labDone.load())
    return;
  labActive = false;
  labEndReason = labStopRequested.load() ? "stopped" : "completed";
  if (!resetInProgress)
    resumeObservations();
  const uint32_t errors = labErrors.load();
  logEvent("LAB_BEACON_DONE", -1, uint8_t(errors > 255 ? 255 : errors));
}

void clearTelemetryState() {
  memset(raw, 0, sizeof(raw));
  memset(levels, 0, sizeof(levels));
  memset(history, 0, sizeof(history));
  memset(events, 0, sizeof(events));
  totalEvents = 0;
  eventHead = 0;
  eventCount = 0;
  if (frameRing)
    memset(frameRing, 0, sizeof(FrameView) * FRAME_RING_CAPACITY);
  frameHead = 0;
  frameCount = 0;
  frameTotal = 0;
  for (uint8_t i = 0; i < BLE_CAPACITY; ++i)
    bleRecords[i] = BleRecord();
  bleCount = 0;
  for (uint8_t i = 0; i < PROBE_CAPACITY; ++i)
    probeRecords[i] = ProbeRecord();
  probeCount = 0;
  for (uint16_t i = 0; i < FIELD_CAPACITY; ++i)
    fieldRecords[i] = FieldRecord();
  fieldCount = 0;
  fieldHead = 0;
  for (uint8_t i = 0; i < WIFI_RESULTS; ++i) {
    names[i] = "";
    rssi[i] = 0;
    wifiChannel[i] = 0;
  }
  networks = 0;
  if (intelligenceStorage)
    memset(intelligenceStorage, 0, sizeof(oni::Observation) * FRAME_RING_CAPACITY);
  intelligenceRing.init(intelligenceStorage, intelligenceStorage ? FRAME_RING_CAPACITY : 0);
  for (auto &t : trafficByChannel)
    t = oni::Traffic();
  detector = Detector();
  tracker = oni::Tracker();
  sessionID = attempted = txErrors = invalidFrames = rttCount = 0;
  rttSum = 0;
  rttMin = UINT32_MAX;
  rttMax = 0;
  memset(sentTimes, 0, sizeof(sentTimes));
  if (observationQueue)
    xQueueReset(observationQueue);
  clearSnapshot();
#if ONI_HAS_VENDOR
  vendorOutput = "";
  vendorOffset = 0;
#endif
  frame = 0;
  sweepMs = 0;
  channel = 0;
  alertUntil = 0;
  captureFrames = 0;
  captureCrcRejected = 0;
}

bool purgeTelemetryFiles() {
  if (!fsOK)
    return false;
  bool ok = true;
  auto removeFile = [&](const String &path) {
    if (LittleFS.exists(path) && !LittleFS.remove(path))
      ok = false;
  };
  for (uint8_t i = 0; i < 8; ++i)
    removeFile("/session" + String(i) + ".json");
  for (const char *path : {"/pending.json", "/events.json", "/frames.csv", "/telemetry.json",
                           "/telemetry.csv", "/telemetry-prev.json", "/telemetry.tmp"})
    removeFile(path);
  return ok;
}

void recoverAfterPurgeFailure() {
  resetInProgress = false;
  mqttStop.store(false);
#if ONI_HAS_BLE
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED)
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
#endif
  if (radioOK && !testing && !captureEnabled) {
    radio.powerUp();
    monitorMode();
  }
  if (!diagnosticBusy())
    resumeObservations();
}
void emergencyResetNow() {
  if (resetInProgress)
    return;
  resetInProgress = true;
  telemetryLogging = false;
  udpEnabled = false;
  ghostEnabled = false;
  stopLab();
  pauseObservations();
  stopMacro("");
  mqttStop.store(true);
  uint64_t deadline = nowMs() + 3000;
  while (((labActive && !labDone.load()) || !mqttIdle.load()) && nowMs() < deadline)
    delay(1);
  if ((labActive && !labDone.load()) || !mqttIdle.load()) {
    // Leave monitoring paused until the lab worker acknowledges its stop.
    runtimeError = "Purge aborted: background worker did not stop";
    recoverAfterPurgeFailure();
    return;
  }
  finishLabIfDone();
  testing = false;
  captureEnabled = false;
  if (radioOK) {
    radio.stopListening();
    radio.flush_rx();
    radio.flush_tx();
    radio.powerDown();
  }
#if ONI_HAS_BLE
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED)
    esp_bt_controller_disable();
#endif
  bool filesRemoved = purgeTelemetryFiles();
  clearTelemetryState();
  if (!filesRemoved) {
    runtimeError = "RAM cleared; filesystem purge could not be verified. Device remains awake.";
    recoverAfterPurgeFailure();
    return;
  }
  esp_wifi_stop();
  if (displayOK) {
    screen->fillScreen(0);
    screen->displayOff();
  }
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  rtc_gpio_pullup_en(GPIO_NUM_0);
  rtc_gpio_pulldown_dis(GPIO_NUM_0);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
  Serial.println("PURGE_COMPLETE / deep sleep");
  Serial.flush();
  esp_deep_sleep_start();
}

String diagnosticStatusJson() {
  String s = "{\"hid_available\":" + String(ONI_HAS_HID ? "true" : "false");
  s += ",\"macro_armed\":" + String(macroArmed && nowMs() < macroArmedUntil ? "true" : "false");
  s += ",\"macro_running\":" + String(macroRunning ? "true" : "false");
  s += ",\"macro_runs\":" + String(macroRuns) + ",\"macro_rejected\":" + String(macroRejected);
  s += ",\"macro_error\":" + quoted(macroError);
  s += ",\"macros\":" + macroListJson();
  s += ",\"lab_active\":" + String(labActive ? "true" : "false");
  s += ",\"lab_rate_hz\":" + String(labRateHz) + ",\"lab_duration_ms\":" + String(labDurationMs);
  s += ",\"lab_sent\":" + String(labSent.load()) + ",\"lab_errors\":" + String(labErrors.load());
  s += ",\"lab_channel\":" + String(labChannel);
  s += ",\"lab_state\":" +
       quoted(labActive ? (labStopRequested.load() ? "stopping" : "running") : labEndReason);
  s += ",\"lab_last_error\":" + quoted(esp_err_to_name(labLastError.load()));
  const uint64_t now = nowMs();
  s += ",\"lab_remaining_ms\":" + decimal64(labActive && labUntil > now ? labUntil - now : 0);
  s += ",\"storage_ok\":" + String(fsOK ? "true" : "false");
#if ONI_HAS_HID
  s += ",\"hid_ready\":" + String(hidReady() ? "true" : "false");
#else
  s += ",\"hid_ready\":false";
#endif
  s += ",\"lab_networks\":[";
  for (uint8_t i = 0; i < labSsidCount; ++i) {
    if (i)
      s += ',';
    s += "{\"ssid\":" + quoted(labSsids[i]) + ",\"bssid\":" + quoted(macText(labBssids[i])) +
         ",\"accepted\":" + String(labPerSsid[i].load()) + "}";
  }
  s += "]";
  s += ",\"queue_drops\":" + String(observationDrops.load());
  s += ",\"capture_buffer_ready\":" + String(frameRing ? "true" : "false");
  s += ",\"observation_buffer_ready\":" + String(intelligenceStorage ? "true" : "false");
  s += ",\"ghost_sleeps\":" + String(ghostSleeps) +
       ",\"ghost_last_error\":" + quoted(esp_err_to_name(ghostLastError));
  s += ",\"ws_available\":" + String(ONI_HAS_WS ? "true" : "false");
  s += ",\"mqtt_available\":" + String(ONI_HAS_MQTT ? "true" : "false");
  s += ",\"mqtt_connected\":" + String(mqttConnected.load() ? "true" : "false");
  s +=
      ",\"mqtt_sent\":" + String(mqttSent.load()) + ",\"mqtt_errors\":" + String(mqttErrors.load());
  s += ",\"udp_sent\":" + String(udpSent) + ",\"udp_errors\":" + String(udpErrors);
  s += ",\"logging_enabled\":" + String(telemetryLogging ? "true" : "false");
  s += ",\"runtime_error\":" + quoted(runtimeError);
  s += ",\"lab_max_ssids\":" + String(ONI_LAB_MAX_SSIDS);
#if ONI_HAS_MSC
  s += ",\"msc_ready\":" + String(mscReady ? "true" : "false") +
       ",\"snapshot_ms\":" + decimal64(snapshotMs);
#else
  s += ",\"msc_ready\":false";
#endif
  s += ",\"reset_reason\":" + quoted(String(resetReason));
  s += ",\"lab_ssids\":[";
  for (uint8_t i = 0; i < labSsidCount; ++i) {
    if (i)
      s += ',';
    s += quoted(labSsids[i]);
  }
  return s + "]}";
}

bool diagnosticLabActive() { return labActive; }

void setupHid() {
#if ONI_HAS_HID
  oniKeyboard.begin();
  oniUsbHidReady = true;
#endif
}

void setupUsbCore() {
#if ONI_HAS_MSC || ONI_HAS_VENDOR || ONI_HAS_HID
  USB.productName("flawless");
  USB.manufacturerName("flawless / ONI");
  USB.serialNumber("FLAWLESS-S3");
#if ONI_HAS_VENDOR
  USB.webUSB(true);
  USB.webUSBURL("http://localhost:8000/webusb.html");
#endif
  USB.begin();
#endif
}

void diagnosticSetup() {
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0)
    rtc_gpio_deinit(GPIO_NUM_0);
  pinMode(0, INPUT_PULLUP);
}

void diagnosticTick() {
  if (macroArmed && nowMs() >= macroArmedUntil && !macroRunning)
    macroArmed = false;
  macroTick();
  finishLabIfDone();
  if (resetRequested) {
    resetRequested = false;
    emergencyResetNow();
  }
}

void setupDiagnosticApi() {
  server.on("/macro/read", HTTP_GET, [] {
    if (server.header("X-Oni-Token") != controlToken) {
      server.send(403, "text/plain", "Invalid control token");
      return;
    }
    String id = server.arg("id");
    if (!macroIdValid(id)) {
      server.send(400, "text/plain", "Invalid slot");
      return;
    }
    File file = LittleFS.open(macroPath(uint8_t(id[0] - '0')), "r");
    if (!file) {
      server.send(404, "text/plain", "Empty slot");
      return;
    }
    server.sendHeader("Cache-Control", "no-store");
    server.streamFile(file, "text/plain");
    file.close();
  });
  server.on("/macro/stop", HTTP_POST, [] {
    if (server.header("X-Oni-Token") != controlToken) {
      server.send(403, "text/plain", "Invalid control token");
      return;
    }
    stopMacro("Stopped by user");
    if (!diagnosticBusy())
      resumeObservations();
    server.send(200, "application/json", diagnosticStatusJson());
  });
  server.on("/diagnostics", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", diagnosticStatusJson());
  });
  server.on("/diagnostics/arm", HTTP_POST, []() {
    if (server.header("X-Oni-Token") != controlToken) {
      server.send(403, "text/plain", "Invalid control token");
      return;
    }
    if (diagnosticBusy() || !hidReady()) {
      server.send(409, "text/plain", "USB keyboard unavailable or diagnostic active");
      return;
    }
    armDiagnostics();
    server.send(200, "application/json", diagnosticStatusJson());
  });
  server.on("/macro/save", HTTP_POST, []() {
    if (server.header("X-Oni-Token") != controlToken) {
      server.send(403, "text/plain", "Invalid control token");
      return;
    }
    if (macroRunning) {
      server.send(409, "text/plain", "wait for the active macro to finish");
      return;
    }
    String id = server.arg("id"), script = server.arg("script");
    if (!macroIdValid(id) || !script.length() || script.length() > ONI_MACRO_MAX_SCRIPT ||
        !parseMacroScript(script)) {
      server.send(400, "text/plain", "id 0..5 and valid bounded macro script required");
      return;
    }
    uint8_t macroId = (uint8_t)(id[0] - '0');
    if (!fsOK) {
      server.send(503, "text/plain", "Storage unavailable");
      return;
    }
    String target = macroPath(macroId), temp = target + ".tmp";
    File file = LittleFS.open(temp, "w");
    bool ok = file && file.print(script) == script.length();
    if (file)
      file.close();
    if (ok)
      ok = LittleFS.rename(temp, target);
    if (!ok)
      LittleFS.remove(temp);
    server.send(ok ? 200 : 500, "application/json", diagnosticStatusJson());
  });
  server.on("/macro/run", HTTP_POST, []() {
    if (server.header("X-Oni-Token") != controlToken) {
      server.send(403, "text/plain", "Invalid control token");
      return;
    }
    String id = server.arg("id");
    if (!macroIdValid(id)) {
      server.send(400, "text/plain", "id 0..5 required");
      return;
    }
    bool ok = startMacro((uint8_t)(id[0] - '0'));
    server.send(ok ? 200 : 409, "application/json", diagnosticStatusJson());
  });
  server.on("/lab/start", HTTP_POST, []() {
    if (server.header("X-Oni-Token") != controlToken) {
      server.send(403, "text/plain", "Invalid control token");
      return;
    }
    uint32_t rate = 0, duration = 0;
    String ssids = server.arg("ssids");
    if (!parseUnsigned(server.arg("rate_hz"), 1, ONI_LAB_MAX_RATE_HZ, rate) ||
        !parseUnsigned(server.arg("duration_ms"), 1000, ONI_LAB_MAX_DURATION_MS, duration) ||
        !startLab(ssids, rate, duration)) {
      server.send(400, "text/plain",
                  "Use 1..4 distinct printable SSIDs (1..32 bytes), rate 1..10 Hz, duration "
                  "1000..60000 ms. Stop other diagnostics and retry after any Wi-Fi scan.");
      return;
    }
    server.send(200, "application/json", diagnosticStatusJson());
  });
  server.on("/lab/stop", HTTP_POST, []() {
    if (server.header("X-Oni-Token") != controlToken) {
      server.send(403, "text/plain", "Invalid control token");
      return;
    }
    stopLab();
    server.send(200, "application/json", diagnosticStatusJson());
  });
  server.on("/emergency-reset", HTTP_POST, []() {
    if (server.header("X-Oni-Token") != controlToken) {
      server.send(403, "text/plain", "Invalid control token");
      return;
    }
    if (server.arg("confirm") != "PURGE") {
      server.send(400, "text/plain", "confirm=PURGE required");
      return;
    }
    resetReason = "web";
    resetRequested = true;
    server.send(202, "application/json", "{\"accepted\":true,\"next\":\"deep_sleep\"}");
  });
}
