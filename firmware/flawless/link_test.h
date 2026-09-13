#pragma once
#include <LittleFS.h>
#include "telemetry.h"
// Fixed bench profile: one packet every 200 ms, 2440 MHz, minimum PA.
constexpr uint8_t TEST_CHANNEL = 40;
constexpr uint32_t INTERVAL_MS = 200;
bool testing = false, initiator = false, fsOK = false;
uint32_t sessionID = 0, planned = 100, attempted = 0, txErrors = 0, invalidFrames = 0, rttCount = 0;
uint64_t testStart = 0, nextTx = 0, lastRx = 0, rttSum = 0;
uint32_t sentTimes[oni::MAX_FRAMES] = {}, rttMin = UINT32_MAX, rttMax = 0;
oni::Tracker tracker;
String controlToken;
const uint8_t addrA[6] = "ONIA1", addrB[6] = "ONIB1";
String linkJson() {
  uint32_t denominator = initiator ? attempted : (sessionID ? planned : 0);
  String s = "{\"active\":" + String(testing ? "true" : "false") +
             ",\"role\":" + quoted(initiator ? "initiator" : "responder");
  s += ",\"session\":" + String(sessionID) +
       ",\"channel\":40,\"frequency_mhz\":2440,\"planned\":" + String(planned);
  s += ",\"attempted\":" + String(attempted) + ",\"unique\":" + String(tracker.unique) +
       ",\"duplicates\":" + String(tracker.duplicates);
  s += ",\"reordered\":" + String(tracker.reordered) +
       ",\"missing_observed\":" + String(tracker.missingObserved());
  s += ",\"tx_errors\":" + String(txErrors) + ",\"invalid\":" + String(invalidFrames);
  s += ",\"pdr_percent\":" +
       (denominator ? String(100.0 * tracker.unique / denominator, 2) : String("null"));
  s += ",\"per_percent\":" + (denominator
                                  ? String(100.0 * (denominator - tracker.unique) / denominator, 2)
                                  : String("null"));
  s += ",\"rtt_mean_us\":" + (rttCount ? decimal64(rttSum / rttCount) : String("null"));
  s += ",\"rtt_min_us\":" + (rttCount ? String(rttMin) : String("null")) +
       ",\"rtt_max_us\":" + (rttCount ? String(rttMax) : String("null"));
  s += ",\"storage_ok\":" + String(fsOK ? "true" : "false") +
       ",\"started_uptime_ms\":" + decimal64(testStart);
  return s + ",\"snapshot_uptime_ms\":" + decimal64(nowMs()) + "}";
}
uint8_t rfCommand(uint8_t cmd, const uint8_t *bytes = nullptr, uint8_t length = 0) {
  rfBus.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  digitalWrite(CSN, LOW);
  uint8_t status = rfBus.transfer(cmd);
  for (uint8_t i = 0; i < length; ++i)
    rfBus.transfer(bytes[i]);
  digitalWrite(CSN, HIGH);
  rfBus.endTransaction();
  return status;
}
bool transmitFrame(const oni::Frame &frame) {
  uint8_t bytes[32];
  oni::encode(bytes, frame);
  radio.stopListening();
  radio.flush_tx();
  uint8_t clear = 0x30;
  rfCommand(0x27, &clear, 1);
  rfCommand(0xA0, bytes, 32); // W_TX_PAYLOAD, fixed 32 bytes; auto-ACK disabled.
  digitalWrite(CE, HIGH);
  delayMicroseconds(15);
  digitalWrite(CE, LOW);
  uint32_t start = micros();
  uint8_t status;
  do {
    status = rfCommand(0xff);
    if (status & 0x30)
      break;
    delayMicroseconds(20);
  } while (uint32_t(micros() - start) < 5000);
  rfCommand(0x27, &clear, 1);
  if (!(status & 0x20))
    radio.flush_tx();
  radio.startListening();
  return (status & 0x20) != 0; // TX_DS is local completion, not proof of delivery.
}
void monitorMode() {
  radio.stopListening();
  radio.setAutoAck(false);
  radio.disableCRC();
  radio.flush_rx();
  radio.flush_tx();
  channel = 0;
  sweepStart = nowMs();
  memset(raw, 0, sizeof(raw));
  detector = Detector();
}
void finishTest(const char *reason) {
  if (!testing)
    return;
  testing = false;
  logEvent(reason, TEST_CHANNEL);
  if (fsOK) {
    String report = "{\"schema\":1,\"reason\":" + quoted(reason) + ",\"link\":" + linkJson() +
                    ",\"log\":" + logsJson() + "}";
    File f = LittleFS.open("/pending.json", "w");
    bool ok = false;
    if (f) {
      ok = f.print(report) == report.length();
      f.close();
    }
    if (ok) {
      LittleFS.remove("/session7.json");
      for (int i = 6; i >= 0; --i) {
        String a = "/session" + String(i) + ".json", b = "/session" + String(i + 1) + ".json";
        if (LittleFS.exists(a) && !LittleFS.rename(a, b))
          ok = false;
      }
      if (!LittleFS.rename("/pending.json", "/session0.json"))
        ok = false;
    }
    if (!ok) {
      fsOK = false;
      logEvent("REPORT_WRITE_FAIL");
    }
  }
  if (radioOK)
    monitorMode();
  if (!diagnosticBusy())
    resumeObservations();
}
void startTest(bool tx, uint32_t count) {
  pauseObservations();
  initiator = tx;
  planned = count;
  sessionID = tx ? (esp_random() | 1u) : 0;
  attempted = txErrors = invalidFrames = rttCount = 0;
  tracker = oni::Tracker();
  rttSum = 0;
  rttMin = UINT32_MAX;
  rttMax = 0;
  testStart = nowMs();
  lastRx = 0;
  nextTx = testStart + 200;
  testing = true;
  radio.stopListening();
  radio.setAutoAck(false);
  radio.setRetries(0, 0);
  radio.setCRCLength(RF24_CRC_16);
  radio.disableDynamicPayloads();
  radio.setPayloadSize(32);
  radio.setAddressWidth(5);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_MIN);
  radio.setChannel(TEST_CHANNEL);
  for (int i = 0; i < 6; ++i)
    radio.closeReadingPipe(i);
  radio.openWritingPipe(tx ? addrB : addrA);
  radio.openReadingPipe(1, tx ? addrA : addrB);
  radio.flush_rx();
  radio.flush_tx();
  radio.startListening();
  logEvent(tx ? "TEST_START" : "PEER_ARMED", TEST_CHANNEL);
}
void linkTick() {
  uint64_t now = nowMs();
  for (int n = 0; n < 3 && radio.available(); ++n) {
    uint8_t b[32];
    radio.read(b, 32);
    oni::Frame f;
    if (!oni::decode(b, f) || f.kind != (initiator ? 2 : 1)) {
      ++invalidFrames;
      continue;
    }
    if (!initiator && !sessionID) {
      sessionID = f.session;
      planned = f.total;
    }
    if (f.session != sessionID || f.total != planned ||
        (initiator && (f.seq >= attempted || f.sent != sentTimes[f.seq]))) {
      ++invalidFrames;
      continue;
    }
    uint32_t gaps = tracker.gapEvents;
    bool fresh = tracker.add(f.seq);
    lastRx = now;
    if (tracker.gapEvents != gaps)
      logEvent("SEQUENCE_GAP", TEST_CHANNEL,
               (uint8_t)min(tracker.missingObserved(), uint32_t(255)));
    if (initiator && fresh) {
      uint32_t rtt = micros() - f.sent;
      rttSum += rtt;
      ++rttCount;
      rttMin = min(rttMin, rtt);
      rttMax = max(rttMax, rtt);
    } else if (!initiator && fresh) {
      f.kind = 2;
      delayMicroseconds(500);
      if (!transmitFrame(f))
        ++txErrors;
      ++attempted;
    }
  }
  if (initiator && attempted < planned && now >= nextTx) {
    uint32_t seq = attempted++;
    sentTimes[seq] = micros();
    oni::Frame f = {1, sessionID, seq, planned, sentTimes[seq]};
    if (!transmitFrame(f))
      ++txErrors;
    nextTx = nowMs() + INTERVAL_MS;
  }
  if (initiator && attempted == planned && now >= nextTx + 1800)
    finishTest("COMPLETE");
  else if (!initiator && sessionID && now - lastRx >= 3000)
    finishTest("PEER_IDLE");
  else if (now - testStart >= 130000)
    finishTest("TIME_LIMIT");
}
void setupLinkApi() {
  fsOK = LittleFS.begin(false);
  if (!fsOK)
    logEvent("REPORT_STORAGE_UNAVAILABLE");
  controlToken = String(esp_random(), HEX) + String(esp_random(), HEX);
  const char *headers[] = {"X-Oni-Token"};
  server.collectHeaders(headers, 1);
  server.on("/control-token", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/plain", controlToken);
  });
  server.on("/test/start", HTTP_POST, []() {
    if (server.header("X-Oni-Token") != controlToken) {
      server.send(403, "text/plain", "Invalid control token");
      return;
    }
    if (diagnosticBusy() || !radioOK) {
      server.send(409, "text/plain", "Test running or radio unavailable");
      return;
    }
    String role = server.arg("role"), count = server.arg("count");
    bool digits = count.length() > 0 && count.length() <= 3;
    for (size_t i = 0; i < count.length(); ++i)
      if (count[i] < '0' || count[i] > '9')
        digits = false;
    int value = count.toInt();
    if ((role != "initiator" && role != "responder") || !digits || value < 1 || value > 600) {
      server.send(400, "text/plain", "Role and count 1..600 required");
      return;
    }
    if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
      server.send(409, "text/plain", "Wi-Fi scan in progress; retry shortly");
      return;
    }
    startTest(role == "initiator", value);
    server.send(200, "application/json", linkJson());
  });
  server.on("/test/stop", HTTP_POST, []() {
    if (server.header("X-Oni-Token") != controlToken) {
      server.send(403, "text/plain", "Invalid control token");
      return;
    }
    finishTest("USER_STOP");
    server.send(200, "application/json", linkJson());
  });
  server.on("/storage/init", HTTP_POST, [] {
    if (server.header("X-Oni-Token") != controlToken) {
      server.send(403, "text/plain", "Invalid token");
      return;
    }
    if (fsOK || diagnosticBusy() || server.arg("confirm") != "FORMAT") {
      server.send(409, "text/plain",
                  "Only initialize unmounted storage while idle, with confirm=FORMAT");
      return;
    }
    fsOK = LittleFS.format() && LittleFS.begin(false);
    server.send(fsOK ? 200 : 500, "application/json",
                String("{\"storage_ok\":") + (fsOK ? "true" : "false") + "}");
  });
  server.on("/sessions", HTTP_GET, []() {
    String s = "[";
    bool comma = false;
    for (int i = 0; fsOK && i < 8; ++i) {
      String p = "/session" + String(i) + ".json";
      if (LittleFS.exists(p)) {
        if (comma)
          s += ',';
        s += String(i);
        comma = true;
      }
    }
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", s + "]");
  });
  server.on("/report", HTTP_GET, []() {
    String id = server.arg("id");
    if (id.length() != 1 || id[0] < '0' || id[0] > '7') {
      server.send(400, "text/plain", "Invalid report");
      return;
    }
    File f = LittleFS.open("/session" + id + ".json", "r");
    if (!f) {
      server.send(404, "text/plain", "Report unavailable");
      return;
    }
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("Content-Disposition",
                      "attachment; filename=flawless-session-" + id + ".json");
    server.streamFile(f, "application/json");
    f.close();
  });
}
