#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Arduino_GFX_Library.h>
#include <esp_timer.h>
#include "build_config.h"
#include "analysis.h"
#include "panel.h"
extern bool testing;
extern bool captureEnabled;
bool diagnosticBusy();
const char *runtimeMode();
void pauseObservations();
void resumeObservations();
String linkJson();
void finishTest(const char *reason);
String intelligenceJson();
String intelligenceExportJson();
String advancedStatusJson();
void setupAdvanced();
void setupAdvancedApi();
void advancedTick();
void trafficSample(uint8_t hit, uint8_t trials, uint64_t timestamp);
bool diagnosticButtonRelease(uint32_t heldMs);
bool diagnosticLabActive();

constexpr uint8_t CE = 15, CSN = 14, MOSI_PIN = 11, MISO_PIN = 13, SCK_PIN = 12;
Arduino_DataBus *bus = new Arduino_ESP32QSPI(6, 47, 18, 7, 48, 5);
Arduino_GFX *screen = new Arduino_RM67162(bus, 17, 0);
SPIClass rfBus(HSPI);
RF24 radio(CE, CSN);
WebServer server(80);
Detector detector;
uint8_t raw[126] = {}, levels[126] = {}, history[24][126] = {};
uint32_t frame = 0, sweepMs = 0;
uint32_t loopCount = 0, loopMaxUs = 0;
bool apOK = false;
String bootStage = "startup";
uint64_t sweepStart = 0, alertUntil = 0, nextCheck = 0, nextWifi = 0;
bool radioOK = false, displayOK = false, registerOK = false;
uint32_t registerUs = 0;
uint8_t channel = 0, page = 0;
const char *pages[] = {"SPECTRUM", "WATERFALL", "WI-FI", "EVENTS", "DIAGNOSTICS", "LINK TEST"};
struct Event {
  uint64_t ms;
  const char *type;
  int16_t channel;
  uint8_t value;
};
Event events[64];
uint32_t totalEvents = 0;
uint8_t eventHead = 0, eventCount = 0;
constexpr uint8_t WIFI_RESULTS = 24;
String names[WIFI_RESULTS];
int rssi[WIFI_RESULTS], wifiChannel[WIFI_RESULTS], networks = 0;
uint64_t nowMs() { return esp_timer_get_time() / 1000ULL; }
String decimal64(uint64_t value) {
  char buffer[24];
  snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value);
  return String(buffer);
}
void logEvent(const char *type, int ch = -1, uint8_t value = 0) {
  Event e = {nowMs(), type, (int16_t)ch, value};
  events[eventHead] = e;
  eventHead = (eventHead + 1) % 64;
  if (eventCount < 64)
    ++eventCount;
  ++totalEvents;
  Serial.printf("{\"uptime_ms\":%llu,\"type\":\"%s\",\"channel\":%d,\"value\":%u}\n",
                (unsigned long long)e.ms, type, ch, value);
}
void hardwareCheck() {
  if (!radioOK || testing)
    return;
  radio.stopListening();
  uint8_t saved = radio.getChannel();
  uint32_t start = micros();
  radio.setChannel(40);
  registerOK = radio.isChipConnected() && radio.getChannel() == 40;
  radio.setChannel(saved);
  registerOK = registerOK && radio.getChannel() == saved;
  registerUs = micros() - start;
  if (captureEnabled)
    radio.startListening();
  logEvent(registerOK ? "REGISTER_PASS" : "REGISTER_FAIL");
}
void sampleChannel() {
  radio.stopListening();
  radio.setChannel(channel);
  uint8_t hits = 0;
  for (int i = 0; i < 12; ++i) {
    radio.startListening();
    delayMicroseconds(180);
    if (radio.testRPD())
      ++hits;
    radio.stopListening();
  }
  raw[channel] = hits * 100 / 12;
  trafficSample(hits, 12, uint64_t(esp_timer_get_time()));
  if (++channel == 126) {
    channel = 0;
    for (int i = 0; i < 126; ++i)
      levels[i] = (levels[i] * 3 + raw[i]) / 4;
    memmove(history[1], history[0], 23 * 126);
    memcpy(history[0], raw, 126);
    uint64_t now = nowMs();
    sweepMs = now - sweepStart;
    sweepStart = now;
    ++frame;
    // One observation per type per sweep avoids flooding the bounded log.
    bool burst = false, steady = false;
    detector.update(raw, now, [&](const char *type, int ch, uint8_t v) {
      bool &seen = strcmp(type, "BURST") == 0 ? burst : steady;
      if (!seen) {
        logEvent(type, ch, v);
        seen = true;
        alertUntil = now + 3000;
      }
    });
  }
}
void buttonTick() {
  static bool candidate = HIGH, stable = HIGH;
  static uint32_t changed = 0, pressed = 0;
  bool reading = digitalRead(0);
  if (reading != candidate) {
    candidate = reading;
    changed = millis();
  }
  if (candidate != stable && millis() - changed >= 35) {
    stable = candidate;
    if (!stable)
      pressed = millis();
    else if (!diagnosticButtonRelease(millis() - pressed)) {
      page = (page + 1) % 6;
      logEvent("PAGE", page);
    }
  }
}
void wifiTick() {
  if (diagnosticBusy())
    return;
  int result = WiFi.scanComplete();
  if (result >= 0) {
    networks = min(result, int(WIFI_RESULTS));
    for (int i = 0; i < networks; ++i) {
      names[i] = WiFi.SSID(i);
      rssi[i] = WiFi.RSSI(i);
      wifiChannel[i] = WiFi.channel(i);
    }
    WiFi.scanDelete();
    nextWifi = nowMs() + 30000;
  } else if (result == WIFI_SCAN_FAILED && nowMs() >= nextWifi) {
    WiFi.scanNetworks(true, false, true, 120);
    nextWifi = nowMs() + 30000;
  }
}
String quoted(const String &s) {
  String out = "\"";
  for (size_t i = 0; i < s.length(); ++i) {
    unsigned char c = s[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += (char)c;
    } else if (c < 32) {
      char b[7];
      snprintf(b, sizeof(b), "\\u%04x", c);
      out += b;
    } else
      out += (char)c;
  }
  return out + "\"";
}
String dataJson() {
  String out;
  out.reserve(1800);
  out = "{\"frame\":" + String(frame) + ",\"uptime_ms\":" + decimal64(nowMs());
  out += ",\"sweep_ms\":" + String(sweepMs) + ",\"radio_ok\":" + String(radioOK ? "true" : "false");
  out += ",\"register_ok\":" + String(registerOK ? "true" : "false") +
         ",\"register_us\":" + String(registerUs);
  out += ",\"mode\":" + quoted(runtimeMode());
  out += ",\"firmware\":" + quoted(FLAWLESS_VERSION) +
         ",\"project\":\"flawless\",\"display_ok\":" + String(displayOK ? "true" : "false") +
         ",\"ap_ok\":" + String(apOK ? "true" : "false");
  out += ",\"boot_stage\":" + quoted(bootStage) +
         ",\"reset_reason\":" + String(int(esp_reset_reason())) +
         ",\"loop_count\":" + String(loopCount) + ",\"loop_max_us\":" + String(loopMaxUs) +
         ",\"psram_bytes\":" + String(ESP.getPsramSize());
  out += ",\"heap_bytes\":" + String(ESP.getFreeHeap()) + ",\"rf_rtt_ms\":null,\"rf_pdr\":null";
  out += ",\"alert\":" + String(nowMs() < alertUntil ? "true" : "false") + ",\"values\":[";
  for (int i = 0; i < 126; ++i) {
    if (i)
      out += ',';
    out += String(levels[i]);
  }
  out += "],\"networks\":[";
  for (int i = 0; i < networks; ++i) {
    if (i)
      out += ',';
    out += "{\"name\":" + quoted(names[i]) + ",\"channel\":" + String(wifiChannel[i]) +
           ",\"rssi\":" + String(rssi[i]) + "}";
  }
  return out + "],\"link_test\":" + linkJson() + ",\"advanced\":" + advancedStatusJson() + "}";
}
String logsJson() {
  String out =
      "{\"clock\":\"boot_relative_ms\",\"overwritten\":" + String(totalEvents - eventCount) +
      ",\"events\":[";
  for (int i = 0; i < eventCount; ++i) {
    Event &e = events[(eventHead + 64 - eventCount + i) % 64];
    if (i)
      out += ',';
    out += "{\"uptime_ms\":" + decimal64(e.ms) + ",\"type\":" + quoted(e.type);
    out += ",\"channel\":" + String(e.channel) + ",\"value\":" + String(e.value) + "}";
  }
  return out + "]}";
}
#include "link_test.h"
#include "advanced_modules.h"
void trafficSample(uint8_t hit, uint8_t trials, uint64_t timestamp) {
  trafficByChannel[channel].sample(hit, trials, timestamp);
}

void lineText(int x, int y, const String &text, uint16_t color = 0xffff) {
  screen->setCursor(x, y);
  screen->setTextColor(color);
  screen->print(text);
}
void render() {
  if (!displayOK)
    return;
  screen->fillScreen(0);
  screen->setTextSize(1);
  screen->fillTriangle(6, 3, 16, 26, 29, 24, 0xffff);
  screen->fillTriangle(530, 3, 520, 26, 507, 24, 0xffff);
  lineText(36, 10, "flawless / " + String(pages[page]));
  lineText(355, 10, testing ? "BOOT: NEXT | TEST" : "BOOT: NEXT | MONITOR");
  screen->drawFastHLine(12, 30, 512, 0x4208);
  if (page < 2) {
    const int x = 40, y = 53, w = 476, h = 132;
    for (int t = 0; t <= 5; ++t) {
      int px = x + t * w / 5;
      screen->drawFastVLine(px, y, h, 0x2104);
      lineText(px - 12, y + h + 10, String(2400 + 25 * t));
    }
    if (page == 0) {
      for (int t = 0; t <= 4; ++t) {
        int py = y + h - t * h / 4;
        screen->drawFastHLine(x, py, w, 0x2104);
        lineText(9, py - 3, String(t * 25));
      }
      for (int i = 0; i < 126; ++i) {
        int px = x + i * (w - 1) / 125, bh = levels[i] * h / 100;
        if (bh)
          screen->drawFastVLine(px, y + h - bh, bh, 0xffff);
      }
      lineText(10, 40, "ACT %");
    } else {
      for (int row = 0; row < 24; ++row)
        for (int i = 0; i < 126; ++i) {
          uint8_t v = history[row][i] * 255 / 100;
          screen->fillRect(x + i * w / 126, y + row * h / 24, 4, 6, screen->color565(v, v, v));
        }
      lineText(8, 42, "NOW");
    }
    lineText(480, 212, "MHz");
  } else if (page == 2) {
    for (int i = 0; i < min(networks, 7); ++i) {
      lineText(14, 48 + i * 22, names[i].substring(0, 32));
      lineText(300, 48 + i * 22, "CH " + String(wifiChannel[i]) + "  " + String(rssi[i]) + " dBm");
    }
    if (!networks)
      lineText(14, 65, "Scanning / no networks");
  } else if (page == 3) {
    for (int i = 0; i < min((int)eventCount, 7); ++i) {
      Event &e = events[(eventHead + 63 - i) % 64];
      lineText(14, 48 + i * 22, decimal64(e.ms) + " ms  " + e.type + "  ch " + String(e.channel));
    }
  } else if (page == 4) {
    lineText(14, 50, "RADIO: " + String(radioOK ? "CONNECTED" : "NOT FOUND"));
    lineText(14, 74, "SPI REGISTER: " + String(registerOK ? "PASS" : "FAIL / UNAVAILABLE"));
    lineText(14, 98, "REGISTER TEST: " + String(registerUs) + " us (NOT RF RTT)");
    lineText(14, 122, "SWEEP: " + String(sweepMs) + " ms / HEAP: " + String(ESP.getFreeHeap()));
    lineText(14, 146, "LINK METRICS: OPEN LINK TEST PAGE");
    lineText(14, 170, "USB SERIAL: d = register test");
    lineText(14, 194, "SIGNATURE: " + dominantSignature());
  }
  if (page == 5) {
    lineText(14, 50,
             String(testing ? "ACTIVE / " : "IDLE / ") + (initiator ? "INITIATOR" : "RESPONDER"));
    lineText(14, 74, "2440 MHz | 1 Mbps | 32 bytes | MIN PA");
    lineText(14, 98, "TX " + String(attempted) + " / RX UNIQUE " + String(tracker.unique));
    lineText(14, 122,
             "OBSERVED GAPS " + String(tracker.missingObserved()) + " / DUP " +
                 String(tracker.duplicates));
    lineText(14, 146,
             rttCount ? "RTT AVG " + decimal64(rttSum / rttCount) + " us"
                      : "RTT: awaiting replies");
    lineText(14, 170, "START / STOP: WEB PANEL > LINK TEST");
  }
  lineText(14, 230,
           radioOK ? "192.168.4.1 | frame " + String(frame) : "NRF24 NOT FOUND - check wiring");
  if (nowMs() < alertUntil)
    lineText(380, 230, "ACTIVITY CHANGE");
}
void setup() {
  Serial.begin(115200);
  Serial.println("flawless " FLAWLESS_VERSION);
  bootStage = "display";
  displayOK = screen->begin(40000000);
  if (displayOK) {
    screen->setRotation(1);
    screen->setTextWrap(false);
  }
  rfBus.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CSN);
  bootStage = "nrf24";
  radioOK = FLAWLESS_ENABLE_NRF && radio.begin(&rfBus) && radio.isChipConnected();
  if (radioOK) {
    radio.setAutoAck(false);
    radio.disableCRC();
    radio.setDataRate(RF24_1MBPS);
    radio.flush_tx();
    radio.flush_rx();
    hardwareCheck();
  }
  logEvent(radioOK ? "BOOT_RX" : "RADIO_MISSING");
  if (!displayOK)
    logEvent("DISPLAY_FAIL");
  bootStage = "access_point";
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  String apName = "flawless-" + String((uint32_t)(ESP.getEfuseMac() & 0xffffff), HEX);
  Serial.println("AP: " + apName);
  apOK = WiFi.softAP(apName.c_str(), "observe24", 1, false, 2);
  if (!apOK)
    logEvent("AP_FAIL");
  server.on("/", []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", PANEL);
  });
  server.on("/data", []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", dataJson());
  });
  server.on("/events", []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", logsJson());
  });
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  bootStage = "storage";
  setupLinkApi();
  setupAdvancedApi();
  bootStage = "advanced";
  setupAdvanced();
  server.begin();
  bootStage = "ready";
  sweepStart = nowMs();
  render();
  Serial.println("READY / http://192.168.4.1 / " FLAWLESS_VERSION);
}
void loop() {
  uint32_t loopStart = micros();
  ++loopCount;
  buttonTick();
  server.handleClient();
  if (radioOK) {
    if (testing)
      linkTick();
    else if (!diagnosticBusy())
      sampleChannel();
  } else
    delay(2);
  if (!diagnosticBusy())
    wifiTick();
  advancedTick();
  if (Serial.available()) {
    char command = Serial.read();
    if (command == 'd')
      hardwareCheck();
    // Explicit provisioning only; ordinary boot never erases the filesystem.
    if (command == 'F' && !diagnosticBusy() && !fsOK) {
      fsOK = LittleFS.format() && LittleFS.begin(false);
      logEvent(fsOK ? "STORAGE_READY" : "STORAGE_FAIL");
    }
  }
  uint64_t now = nowMs();
  if (now >= nextCheck) {
    nextCheck = now + 5000;
    if (radioOK && !radio.isChipConnected()) {
      radioOK = false;
      captureEnabled = false;
      logEvent("RADIO_LOST");
      finishTest("RADIO_LOST");
      if (!diagnosticBusy())
        resumeObservations();
    }
  }
  static uint64_t nextDraw = 0;
  if (now >= nextDraw) {
    nextDraw = now + 500;
    render();
  }
  loopMaxUs = max(loopMaxUs, uint32_t(micros() - loopStart));
  delay(1);
}
