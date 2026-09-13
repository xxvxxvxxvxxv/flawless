#pragma once
/* Optional, local-first intelligence extensions. Hardware-specific APIs are
   guarded so the base monitor remains buildable when a library is absent. */
#include "intelligence_model.h"
#include "fat12_snapshot.h"
#include <atomic>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <esp_bt.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <esp_heap_caps.h>
#if FLAWLESS_ENABLE_MQTT
#include <PubSubClient.h>
#define ONI_HAS_MQTT 1
#else
#define ONI_HAS_MQTT 0
#endif
#if FLAWLESS_ENABLE_WS
#include <WebSocketsServer.h>
#define ONI_HAS_WS 1
#else
#define ONI_HAS_WS 0
#endif
#if FLAWLESS_ENABLE_BLE
#include <BLEDevice.h>
#include <BLEScan.h>
#define ONI_HAS_BLE 1
#else
#define ONI_HAS_BLE 0
#endif
#if FLAWLESS_USB_OTG && CONFIG_TINYUSB_MSC_ENABLED
#include <USB.h>
#include <USBMSC.h>
#define ONI_HAS_MSC 1
#else
#define ONI_HAS_MSC 0
#endif
#if FLAWLESS_USB_OTG && CONFIG_TINYUSB_VENDOR_ENABLED
#include <USB.h>
#include <USBVendor.h>
#define ONI_HAS_VENDOR 1
#else
#define ONI_HAS_VENDOR 0
#endif

constexpr uint8_t CAPTURE_CHANNEL = 40;
constexpr uint16_t FRAME_RING_CAPACITY = 1000;
constexpr uint16_t BLE_CAPACITY = 32;
constexpr uint16_t PROBE_CAPACITY = 64;
constexpr uint16_t FIELD_CAPACITY = 128;
constexpr uint16_t UDP_PORT = 39001;

struct BleRecord {
  String address, name, manufacturer;
  int rssi = 0;
  uint64_t seen = 0;
};
struct ProbeRecord {
  uint8_t mac[6] = {};
  String ssid;
  int rssi = 0;
  uint64_t seen = 0;
  uint32_t count = 0;
};
struct FieldRecord {
  String marker;
  uint64_t uptime = 0;
  uint8_t peakChannel = 0, peakValue = 0;
  int16_t rssi = 0;
};
struct FrameView {
  uint64_t uptime = 0;
  uint8_t length = 0, crcOk = 1, channel = CAPTURE_CHANNEL;
  uint8_t payload[32] = {};
};

oni::Observation *intelligenceStorage = nullptr;
oni::Ring<oni::Observation> intelligenceRing;
BleRecord bleRecords[BLE_CAPACITY];
uint8_t bleCount = 0;
ProbeRecord probeRecords[PROBE_CAPACITY];
uint8_t probeCount = 0;
FieldRecord fieldRecords[FIELD_CAPACITY];
uint16_t fieldCount = 0, fieldHead = 0;
FrameView *frameRing = nullptr;
uint16_t frameHead = 0, frameCount = 0;
uint32_t frameTotal = 0;
oni::Traffic trafficByChannel[126];
WiFiUDP udpTelemetry;
bool udpEnabled = false;
uint32_t udpIntervalMs = 1000;
uint64_t nextUdp = 0;
#if ONI_HAS_MQTT
WiFiClient mqttNet;
PubSubClient mqttClient(mqttNet);
bool mqttEnabled = false;
String mqttHost = "";
uint16_t mqttPort = 1883;
String mqttTopic = "flawless/telemetry";
uint64_t nextMqtt = 0;
#endif
bool captureEnabled = false, ghostEnabled = false;
uint32_t captureFrames = 0, captureCrcRejected = 0;
uint8_t captureAddress[6] = {'O', 'N', 'I', 'C', '1', 0};
#if ONI_HAS_WS
WebSocketsServer wsTelemetry(81);
#endif
// Producers only enqueue fixed-size observations. The loop owns all records.
QueueHandle_t observationQueue = nullptr;
std::atomic<bool> observationsEnabled{false};
std::atomic<uint32_t> observationDrops{0};
uint32_t udpSent = 0, udpErrors = 0;
bool telemetryLogging = false;
uint64_t nextJournal = 0;
String runtimeError = "";
uint32_t ghostSleeps = 0;
int ghostLastError = 0;
bool diagnosticBusy();
void enqueueObservation(const oni::Observation &o) {
  if (observationsEnabled.load() && observationQueue &&
      xQueueSend(observationQueue, &o, 0) != pdTRUE)
    ++observationDrops;
}
#if ONI_HAS_BLE
class OniBleCallbacks : public BLEAdvertisedDeviceCallbacks {
public:
  void onResult(BLEAdvertisedDevice d) override {
    if (!observationsEnabled.load())
      return;
    oni::Observation o;
    o.us = uint64_t(esp_timer_get_time());
    o.layer = oni::BLE;
    o.rssi = d.getRSSI();
    o.channel = 255;
    auto addr = d.getAddress().toString();
    for (int i = 0; i < 6; ++i)
      o.mac[i] = uint8_t(strtoul(addr.c_str() + i * 3, nullptr, 16));
    if (d.haveName()) {
      auto name = d.getName();
      strncpy(o.name, name.c_str(), 32);
    }
    if (d.haveManufacturerData()) {
      auto m = d.getManufacturerData();
      o.length = uint8_t(m.length() > 32 ? 32 : m.length());
      memcpy(o.bytes, m.c_str(), o.length); // Preserve embedded zero bytes.
    }
    strcpy(o.kind, "BLE_ADVERTISEMENT");
    enqueueObservation(o);
  }
};
OniBleCallbacks bleCallbacks;
BLEScan *bleScan = nullptr;
#endif

String macText(const uint8_t *m) {
  char b[18];
  snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
  return String(b);
}
String hexText(const uint8_t *p, size_t n) {
  String s;
  for (size_t i = 0; i < n; ++i) {
    char b[3];
    snprintf(b, sizeof(b), "%02X", p[i]);
    if (i)
      s += ' ';
    s += b;
  }
  return s;
}
String asciiText(const uint8_t *p, size_t n) {
  String s;
  for (size_t i = 0; i < n; ++i)
    s += (p[i] >= 32 && p[i] < 127) ? char(p[i]) : '.';
  return s;
}
String diagnosticStatusJson();
bool diagnosticLabActive();
void pushObservation(oni::Observation o) { intelligenceRing.push(o); }
void pushFrame(const uint8_t *payload, uint8_t len, bool crcOk) {
  if (!frameRing)
    return;
  if (len > 32)
    len = 32;
  FrameView &v = frameRing[frameHead];
  v.uptime = nowMs();
  v.length = len;
  v.crcOk = uint8_t(crcOk);
  v.channel = CAPTURE_CHANNEL;
  memset(v.payload, 0, sizeof(v.payload));
  memcpy(v.payload, payload, len);
  frameHead = (frameHead + 1) % FRAME_RING_CAPACITY;
  if (frameCount < FRAME_RING_CAPACITY)
    ++frameCount;
  ++frameTotal;
  oni::Observation o;
  o.us = uint64_t(esp_timer_get_time());
  o.layer = oni::NRF;
  o.channel = CAPTURE_CHANNEL;
  o.length = len;
  o.crc = crcOk;
  memcpy(o.bytes, payload, len);
  strncpy(o.kind, "NRF_PAYLOAD", 23);
  o.kind[23] = 0;
  pushObservation(o);
  trafficByChannel[CAPTURE_CHANNEL].packet(len, o.us);
#if ONI_HAS_WS
  String msg = "{\"type\":\"frame\",\"uptime_ms\":" + decimal64(v.uptime) +
               ",\"channel\":40,\"length\":" + String(len) +
               ",\"crc_ok\":" + String(crcOk ? "true" : "false") +
               ",\"hex\":" + quoted(hexText(payload, len)) +
               ",\"ascii\":" + quoted(asciiText(payload, len)) + "}";
  wsTelemetry.broadcastTXT(msg);
#endif
}
void captureTick() {
  if (!captureEnabled || !radioOK || testing)
    return;
  for (uint8_t n = 0; n < 3 && radio.available(); ++n) {
    uint8_t b[32] = {};
    uint8_t pipe = 0;
    bool available = radio.available(&pipe);
    if (!available)
      break;
    radio.read(b, 32);
    pushFrame(b, 32, true);
    ++captureFrames;
  }
}
#if ONI_HAS_BLE
void bleScanComplete(BLEScanResults) {
  // Results are cleared by the loop before starting the next scan.
}
void bleTick() {
  if (!bleScan || diagnosticBusy() || !observationsEnabled.load())
    return;
  static uint64_t next = 0;
  if (nowMs() < next)
    return;
  next = nowMs() + 3500;
  if (!bleScan->isScanning()) {
    bleScan->clearResults();
    // The callback form returns immediately; the old overload blocked loop()
    // for the full two-second scan and starved the display/web server.
    bleScan->start(2, bleScanComplete, false);
  }
}
#endif

void promiscuousCallback(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (!observationsEnabled.load() || type != WIFI_PKT_MGMT || !buf)
    return;
  const auto *p = static_cast<const wifi_promiscuous_pkt_t *>(buf);
  size_t len = p->rx_ctrl.sig_len;
  if (len < 4)
    return;
  len -= 4;
  oni::Observation o;
  o.us = uint64_t(esp_timer_get_time());
  o.layer = oni::WIFI;
  o.rssi = p->rx_ctrl.rssi;
  o.channel = p->rx_ctrl.channel;
  if (oni::parseManagement(p->payload, len, o) && strcmp(o.kind, "PROBE_REQUEST") == 0)
    enqueueObservation(o);
}
bool promiscActive = false;
void wifiPromisc(bool enabled) {
  if (enabled == promiscActive)
    return;
  esp_err_t e = esp_wifi_set_promiscuous(enabled);
  if (e == ESP_OK) {
    esp_wifi_set_promiscuous_rx_cb(enabled ? promiscuousCallback : nullptr);
    promiscActive = enabled;
  } else
    runtimeError = String("promiscuous: ") + esp_err_to_name(e);
}
void drainObservations() {
  oni::Observation o;
  for (uint8_t n = 0;
       observationQueue && n < 16 && xQueueReceive(observationQueue, &o, 0) == pdTRUE; ++n) {
    if (!observationsEnabled.load())
      continue;
    if (o.layer == oni::BLE) {
      String addr = macText(o.mac);
      int found = -1;
      for (uint8_t i = 0; i < bleCount; ++i)
        if (bleRecords[i].address == addr) {
          found = i;
          break;
        }
      if (found < 0) {
        if (bleCount < BLE_CAPACITY)
          found = bleCount++;
        else {
          found = 0;
          for (uint8_t i = 1; i < bleCount; ++i)
            if (bleRecords[i].seen < bleRecords[found].seen)
              found = i;
        }
      }
      auto &r = bleRecords[found];
      r.address = addr;
      r.name = o.name;
      r.rssi = o.rssi;
      r.seen = o.us / 1000;
      r.manufacturer = hexText(o.bytes, o.length);
    } else if (o.layer == oni::WIFI) {
      int found = -1;
      for (uint8_t i = 0; i < probeCount; ++i)
        if (!memcmp(probeRecords[i].mac, o.mac, 6) && probeRecords[i].ssid == o.name) {
          found = i;
          break;
        }
      if (found < 0) {
        if (probeCount < PROBE_CAPACITY)
          found = probeCount++;
        else {
          found = 0;
          for (uint8_t i = 1; i < probeCount; ++i)
            if (probeRecords[i].seen < probeRecords[found].seen)
              found = i;
        }
        probeRecords[found] = ProbeRecord();
        memcpy(probeRecords[found].mac, o.mac, 6);
        probeRecords[found].ssid = o.name;
      }
      auto &r = probeRecords[found];
      r.rssi = o.rssi;
      r.seen = o.us / 1000;
      ++r.count;
    }
    pushObservation(o);
  }
}
void pauseObservations() {
  observationsEnabled.store(false);
  wifiPromisc(false);
#if ONI_HAS_BLE
  if (bleScan && bleScan->isScanning())
    bleScan->stop();
#endif
}
void resumeObservations() {
  if (observationQueue) {
    observationsEnabled.store(true);
    wifiPromisc(true);
  }
}

String frameJson(uint16_t limit = 100) {
  if (!frameRing)
    return "[]";
  String s = "[";
  uint16_t start = frameCount > limit ? frameCount - limit : 0;
  for (uint16_t i = start; i < frameCount; ++i) {
    const FrameView &v =
        frameRing[(frameHead + FRAME_RING_CAPACITY - frameCount + i) % FRAME_RING_CAPACITY];
    if (i > start)
      s += ',';
    s += "{\"uptime_ms\":" + decimal64(v.uptime) + ",\"channel\":" + String(v.channel) +
         ",\"length\":" + String(v.length) + ",\"crc_ok\":" + String(v.crcOk ? "true" : "false") +
         ",\"hex\":" + quoted(hexText(v.payload, v.length)) +
         ",\"ascii\":" + quoted(asciiText(v.payload, v.length)) + "}";
  }
  return s + "]";
}
String bleJson() {
  String s = "[";
  for (uint8_t i = 0; i < bleCount; ++i) {
    if (i)
      s += ',';
    s += "{\"address\":" + quoted(bleRecords[i].address) +
         ",\"name\":" + quoted(bleRecords[i].name) + ",\"rssi\":" + String(bleRecords[i].rssi) +
         ",\"manufacturer\":" + quoted(bleRecords[i].manufacturer) +
         ",\"seen_uptime_ms\":" + decimal64(bleRecords[i].seen) + "}";
  }
  return s + "]";
}
String probesJson() {
  String s = "[";
  for (uint8_t i = 0; i < probeCount; ++i) {
    if (i)
      s += ',';
    s += "{\"mac\":" + quoted(macText(probeRecords[i].mac)) +
         ",\"ssid\":" + quoted(probeRecords[i].ssid) + ",\"rssi\":" + String(probeRecords[i].rssi) +
         ",\"count\":" + String(probeRecords[i].count) +
         ",\"seen_uptime_ms\":" + decimal64(probeRecords[i].seen) + "}";
  }
  return s + "]";
}
String dominantSignature() {
  uint8_t best = 0;
  for (uint8_t i = 1; i < 126; ++i)
    if (trafficByChannel[i].packets > trafficByChannel[best].packets)
      best = i;
  return String("CH ") + String(best) + " " + String(trafficByChannel[best].signature());
}
String trafficJson() {
  String s = "[";
  for (int i = 0; i < 126; ++i) {
    if (i)
      s += ',';
    s += "{\"channel\":" + String(i) +
         ",\"occupancy_percent\":" + String(trafficByChannel[i].occupancy(), 2) +
         ",\"packets\":" + String(trafficByChannel[i].packets) + ",\"mean_iat_us\":" +
         (trafficByChannel[i].iatCount ? String(trafficByChannel[i].meanIat, 1) : String("null")) +
         ",\"burst_edges\":" + String(trafficByChannel[i].bursts) +
         ",\"profile\":" + quoted(trafficByChannel[i].profile()) +
         ",\"signature\":" + quoted(trafficByChannel[i].signature()) + "}";
  }
  return s + "]";
}
String fieldsJson() {
  String s = "[";
  for (uint16_t i = 0; i < fieldCount; ++i) {
    if (i)
      s += ',';
    auto &f = fieldRecords[(fieldHead + FIELD_CAPACITY - fieldCount + i) % FIELD_CAPACITY];
    s += "{\"marker\":" + quoted(f.marker) + ",\"uptime_ms\":" + decimal64(f.uptime) +
         ",\"peak_channel\":" + String(f.peakChannel) + ",\"peak_value\":" + String(f.peakValue) +
         ",\"rssi\":" + String(f.rssi) + "}";
  }
  return s + "]";
}
String advancedStatusJson() {
  return "{\"capture_active\":" + String(captureEnabled ? "true" : "false") +
         ",\"ghost_enabled\":" + String(ghostEnabled ? "true" : "false") +
         ",\"udp_enabled\":" + String(udpEnabled ? "true" : "false") + ",\"mqtt_enabled\":" +
         String(
#if ONI_HAS_MQTT
             mqttEnabled ? "true" : "false"
#else
             "false"
#endif
             ) +
         ",\"ble_count\":" + String(bleCount) + ",\"probe_count\":" + String(probeCount) +
         ",\"diagnostics\":" + diagnosticStatusJson() + "}";
}
String observationsJson(uint16_t limit = 1000) {
  String s = "[";
  size_t start = intelligenceRing.count > limit ? intelligenceRing.count - limit : 0;
  for (size_t i = start; i < intelligenceRing.count; ++i) {
    const oni::Observation &o = intelligenceRing.at(i);
    if (i > start)
      s += ',';
    s += "{\"id\":" + String(o.id) + ",\"uptime_us\":" + decimal64(o.us) +
         ",\"layer\":" + String(o.layer) + ",\"channel\":" + String(o.channel) +
         ",\"length\":" + String(o.length) + ",\"rssi\":" +
         ((o.layer == oni::BLE || o.layer == oni::WIFI) ? String(o.rssi) : String("null")) +
         ",\"crc_ok\":" + String(o.layer == oni::NRF ? (o.crc ? "true" : "false") : "null") +
         ",\"kind\":" + quoted(String(o.kind)) + ",\"name\":" + quoted(String(o.name)) +
         ",\"hex\":" + quoted(hexText(o.bytes, o.length)) + "}";
  }
  return s + "]";
}
String intelligenceJsonInternal(uint16_t limit) {
  return "{\"observations\":" + observationsJson(limit) + ",\"frames\":" + frameJson(limit) +
         ",\"ble\":" + bleJson() + ",\"wifi_probes\":" + probesJson() +
         ",\"traffic\":" + trafficJson() + ",\"field\":" + fieldsJson() +
         ",\"frame_total\":" + String(frameTotal) + ",\"capture_crc_rejected\":null}";
}
String intelligenceJson() { return intelligenceJsonInternal(200); }
String intelligenceExportJson() { return intelligenceJsonInternal(1000); }

uint8_t peakChannel();
String activityJson() {
  String s = "{\"uptime_ms\":" + decimal64(nowMs()) + ",\"frame\":" + String(frame) +
             ",\"peak_channel\":" + String(peakChannel()) +
             ",\"alert\":" + String(nowMs() < alertUntil ? "true" : "false") +
             ",\"event_count\":" + String(eventCount) + ",\"values\":[";
  for (int i = 0; i < 126; ++i) {
    if (i)
      s += ',';
    s += String(levels[i]);
  }
  return s + "]}";
}
uint8_t peakChannel() {
  uint8_t p = 0;
  for (uint8_t i = 1; i < 126; ++i)
    if (levels[i] > levels[p])
      p = i;
  return p;
}
void udpTick() {
  if (!udpEnabled || nowMs() < nextUdp ||
      (!WiFi.softAPgetStationNum() && WiFi.status() != WL_CONNECTED))
    return;
  nextUdp = nowMs() + udpIntervalMs;
  String s = activityJson();
  if (udpTelemetry.beginPacket(IPAddress(192, 168, 4, 255), UDP_PORT) &&
      udpTelemetry.print(s) == s.length() && udpTelemetry.endPacket())
    ++udpSent;
  else
    ++udpErrors;
}
std::atomic<bool> mqttStop{false}, mqttIdle{true}, mqttConnected{false};
std::atomic<uint32_t> mqttSent{0}, mqttErrors{0};
#if ONI_HAS_MQTT
struct MqttJob {
  char host[128], topic[97], payload[1024];
  uint16_t port;
  bool enabled;
};
QueueHandle_t mqttQueue = nullptr;
TaskHandle_t mqttTaskHandle = nullptr;
void mqttWorker(void *) {
  MqttJob job{};
  bool have = false;
  char clientId[32];
  snprintf(clientId, sizeof(clientId), "flawless-%06lx",
           (unsigned long)(ESP.getEfuseMac() & 0xffffff));
  mqttClient.setBufferSize(1400);
  mqttClient.setSocketTimeout(1);
  mqttNet.setConnectionTimeout(1000);
  for (;;) {
    if (mqttStop.load()) {
      mqttClient.disconnect();
      mqttConnected.store(false);
      mqttIdle.store(true);
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    mqttIdle.store(false);
    MqttJob next{};
    if (xQueueReceive(mqttQueue, &next, pdMS_TO_TICKS(50)) == pdTRUE) {
      bool changed = !have || strcmp(next.host, job.host) || next.port != job.port;
      if (changed)
        mqttClient.disconnect();
      job = next;
      have = true;
      if (!job.enabled) {
        mqttClient.disconnect();
      } else {
        mqttClient.setServer(job.host, job.port);
        if (!mqttClient.connected() && !mqttClient.connect(clientId))
          ++mqttErrors;
        if (mqttClient.connected()) {
          if (mqttClient.publish(job.topic, job.payload))
            ++mqttSent;
          else
            ++mqttErrors;
        }
      }
    }
    if (mqttClient.connected())
      mqttClient.loop();
    mqttConnected.store(mqttClient.connected());
  }
}
void mqttTick() {
  if (!mqttQueue || nowMs() < nextMqtt)
    return;
  nextMqtt = nowMs() + 3000;
  MqttJob job{};
  job.enabled = mqttEnabled;
  job.port = mqttPort;
  snprintf(job.host, sizeof(job.host), "%s", mqttHost.c_str());
  snprintf(job.topic, sizeof(job.topic), "%s", mqttTopic.c_str());
  String payload = activityJson();
  snprintf(job.payload, sizeof(job.payload), "%s", payload.c_str());
  xQueueOverwrite(mqttQueue, &job);
}
bool setupMqttWorker() {
  if (mqttTaskHandle)
    return true;
  if (!mqttQueue)
    mqttQueue = xQueueCreate(1, sizeof(MqttJob));
  return mqttQueue && xTaskCreatePinnedToCore(mqttWorker, "flawless-mqtt", 6144, nullptr, 1,
                                              &mqttTaskHandle, 0) == pdPASS;
}
#else
void mqttTick() {}
#endif

void fieldAdd(const String &marker, int16_t rssiValue) {
  uint8_t peak = 0, pv = 0;
  for (uint8_t i = 0; i < 126; ++i)
    if (levels[i] > pv) {
      pv = levels[i];
      peak = i;
    }
  FieldRecord &f = fieldRecords[fieldHead];
  fieldHead = (fieldHead + 1) % FIELD_CAPACITY;
  if (fieldCount < FIELD_CAPACITY)
    ++fieldCount;
  f.marker = marker;
  f.uptime = nowMs();
  f.peakChannel = peak;
  f.peakValue = pv;
  f.rssi = rssiValue;
  logEvent("FIELD_MARK", peak, pv);
}

String csvCell(const String &text) {
  String out = "\"";
  for (size_t i = 0; i < text.length(); ++i) {
    if (text[i] == '"')
      out += '"';
    out += text[i];
  }
  return out + '"';
}
String frameCsvLine(const FrameView &v) {
  return "nrf24," + decimal64(v.uptime) + "," + String(v.channel) + "," + String(v.length) + "," +
         String(v.crcOk) + "," + csvCell(hexText(v.payload, v.length)) + "," +
         csvCell(asciiText(v.payload, v.length)) + "\r\n";
}
const char *csvHeader = "layer,uptime_ms,channel,length,crc_ok,hex,ascii\r\n";
#if ONI_HAS_MSC
USBMSC msc;
constexpr uint32_t MSC_SECTOR_SIZE = 512, MSC_IMAGE_BYTES = flawless::Fat12Snapshot::SIZE,
                   MSC_SECTORS = MSC_IMAGE_BYTES / 512;
uint8_t *mscImage = nullptr;
SemaphoreHandle_t mscMutex = nullptr;
bool snapshotRequested = false, mscReady = false;
uint64_t snapshotMs = 0;
bool refreshSnapshot() {
  uint8_t *next = (uint8_t *)heap_caps_malloc(MSC_IMAGE_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!next || !mscImage || !mscMutex) {
    if (next)
      free(next);
    return false;
  }
  flawless::Fat12Snapshot disk(next);
  disk.begin();
  String events = logsJson();
  String csv = csvHeader;
  uint16_t first = frameCount > 100 ? frameCount - 100 : 0;
  for (uint16_t i = first; i < frameCount; ++i)
    csv += frameCsvLine(
        frameRing[(frameHead + FRAME_RING_CAPACITY - frameCount + i) % FRAME_RING_CAPACITY]);
  bool ok = disk.add("EVENTS  JSN", events.c_str(), events.length()) &&
            disk.add("FRAMES  CSV", csv.c_str(), csv.length());
  const char *info =
      "flawless snapshot. EVENTS.JSN contains JSON; FRAMES.CSV contains the latest 100 frames. "
      "Full exports are on the dashboard. Eject before requesting refresh.\r\n";
  ok = ok && disk.add("README  TXT", info, strlen(info));
  if (ok) {
    msc.mediaPresent(false);
    xSemaphoreTake(mscMutex, portMAX_DELAY);
    memcpy(mscImage, next, MSC_IMAGE_BYTES);
    xSemaphoreGive(mscMutex);
    msc.mediaPresent(true);
    snapshotMs = nowMs();
  }
  free(next);
  return ok;
}
static int32_t mscRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t count) {
  uint64_t pos = uint64_t(lba) * 512 + offset;
  if (!mscImage || !mscMutex || pos >= MSC_IMAGE_BYTES || count > MSC_IMAGE_BYTES - pos)
    return -1;
  xSemaphoreTake(mscMutex, portMAX_DELAY);
  memcpy(buffer, mscImage + size_t(pos), count);
  xSemaphoreGive(mscMutex);
  return count;
}
static int32_t mscWrite(uint32_t, uint32_t, uint8_t *, uint32_t) { return -1; }
void mscSetup() {
  if (!mscImage || !mscMutex)
    return;
  msc.vendorID("FLAWLESS");
  msc.productID("SNAPSHOT");
  msc.productRevision("1.0");
  msc.onRead(mscRead);
  msc.onWrite(mscWrite);
  msc.isWritable(false);
  mscReady = msc.begin(MSC_SECTORS, 512);
  if (mscReady)
    refreshSnapshot();
}
void clearSnapshot() {
  if (mscReady)
    msc.mediaPresent(false);
  if (mscImage && mscMutex) {
    xSemaphoreTake(mscMutex, portMAX_DELAY);
    memset(mscImage, 0, MSC_IMAGE_BYTES);
    xSemaphoreGive(mscMutex);
  }
}
#else
void mscSetup() {}
void clearSnapshot() {}
#endif

bool allocateAdvancedBuffers() {
  if (!intelligenceStorage)
    intelligenceStorage = (oni::Observation *)heap_caps_calloc(
        FRAME_RING_CAPACITY, sizeof(oni::Observation), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!frameRing)
    frameRing = (FrameView *)heap_caps_calloc(FRAME_RING_CAPACITY, sizeof(FrameView),
                                              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#if ONI_HAS_MSC
  if (!mscImage)
    mscImage = (uint8_t *)heap_caps_calloc(1, MSC_IMAGE_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
  return intelligenceStorage && frameRing;
}

#if ONI_HAS_VENDOR
USBVendor usbVendor;
String vendorOutput;
size_t vendorOffset = 0;
void usbVendorTick() {
  if (!usbVendor.mounted()) {
    vendorOutput = "";
    vendorOffset = 0;
    return;
  }
  if (vendorOffset < vendorOutput.length()) {
    size_t left = vendorOutput.length() - vendorOffset;
    size_t n = left > 64 ? 64 : left;
    vendorOffset += usbVendor.write((const uint8_t *)vendorOutput.c_str() + vendorOffset, n);
  } else {
    vendorOutput = "";
    vendorOffset = 0;
    if (usbVendor.available()) {
      int c = usbVendor.read();
      if (c == 'd') {
        hardwareCheck();
        vendorOutput = String(registerOK ? "REGISTER_PASS" : "REGISTER_FAIL") + "\n";
      } else if (c == 'i')
        vendorOutput = intelligenceJson() + "\n";
    }
  }
}
void vendorSetup() { usbVendor.begin(); }
#else
void usbVendorTick() {}
void vendorSetup() {}
#endif

#include "diagnostic_modules.h"

void ghostTick() {
  if (!ghostEnabled || diagnosticBusy() || WiFi.softAPgetStationNum() ||
      WiFi.status() == WL_CONNECTED || usbHostMounted())
    return;
  static uint64_t next = 0;
  if (nowMs() < next)
    return;
  next = nowMs() + 10000;
  pauseObservations();
#if ONI_HAS_BLE
  bool btWasEnabled = esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED;
  if (btWasEnabled && esp_bt_controller_disable() != ESP_OK) {
    resumeObservations();
    return;
  }
#endif
  if (radioOK)
    radio.powerDown();
  esp_err_t stopped = esp_wifi_stop();
  if (stopped == ESP_OK) {
    esp_sleep_enable_timer_wakeup(1000000ULL);
    ghostLastError = esp_light_sleep_start();
    if (ghostLastError == ESP_OK)
      ++ghostSleeps;
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    esp_wifi_start();
  } else
    ghostLastError = stopped;
#if ONI_HAS_BLE
  if (btWasEnabled)
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
#endif
  if (radioOK) {
    radio.powerUp();
    monitorMode();
  }
  resumeObservations();
}
void journalTick() {
  if (!telemetryLogging || !fsOK || nowMs() < nextJournal || diagnosticBusy())
    return;
  nextJournal = nowMs() + 30000;
  // Two rotating snapshots, at most 100 observations each; limits flash writes.
  File out = LittleFS.open("/telemetry.tmp", "w");
  if (!out) {
    runtimeError = "journal open failed";
    return;
  }
  String snapshot =
      "{\"events\":" + logsJson() + ",\"observations\":" + observationsJson(100) + "}";
  bool ok = out.print(snapshot) == snapshot.length();
  out.close();
  if (ok) {
    LittleFS.remove("/telemetry-prev.json");
    if (LittleFS.exists("/telemetry.json"))
      ok = LittleFS.rename("/telemetry.json", "/telemetry-prev.json");
    if (ok)
      ok = LittleFS.rename("/telemetry.tmp", "/telemetry.json");
  }
  if (!ok)
    runtimeError = "journal write/rename failed";
}

void setupAdvanced() {
  bool buffersReady = allocateAdvancedBuffers();
  if (!buffersReady)
    Serial.println("PSRAM allocation failed for telemetry buffers");
  intelligenceRing.init(intelligenceStorage, intelligenceStorage ? FRAME_RING_CAPACITY : 0);
  observationQueue = xQueueCreate(32, sizeof(oni::Observation));
#if ONI_HAS_MSC
  mscMutex = xSemaphoreCreateMutex();
#endif
#if ONI_HAS_WS
  wsTelemetry.begin();
#endif
#if ONI_HAS_BLE
  BLEDevice::init("");
  bleScan = BLEDevice::getScan();
  bleScan->setActiveScan(false);
  bleScan->setInterval(80);
  bleScan->setWindow(40);
  bleScan->setAdvertisedDeviceCallbacks(&bleCallbacks, true);
#endif
  resumeObservations();
#if ONI_HAS_MSC
  if (mscImage)
    mscSetup();
#else
  mscSetup();
#endif
  setupHid();
  vendorSetup();
  setupUsbCore();
  diagnosticSetup();
}
void advancedTick() {
  drainObservations();
  captureTick();
#if ONI_HAS_WS
  wsTelemetry.loop();
#endif
#if ONI_HAS_BLE
  bleTick();
#endif
  diagnosticTick();
  udpTick();
  mqttTick();
  ghostTick();
  usbVendorTick();
  journalTick();
#if ONI_HAS_MSC
  if (snapshotRequested) {
    snapshotRequested = false;
    if (!refreshSnapshot())
      runtimeError = "snapshot refresh failed";
  }
#endif
}
void setupAdvancedApi() {
  server.on("/webusb", HTTP_GET, [] {
    server.send(200, "application/json",
                String("{\"available\":") + (ONI_HAS_VENDOR ? "true" : "false") +
                    ",\"commands\":\"d=register test, i=JSON snapshot\"}");
  });
  server.on("/intelligence", HTTP_GET, [] {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", intelligenceJson());
  });
  server.on("/frames", HTTP_GET, [] {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", frameJson());
  });
  server.on("/ble", HTTP_GET, [] {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", bleJson());
  });
  server.on("/wifi-probes", HTTP_GET, [] {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", probesJson());
  });
  server.on("/capture/start", HTTP_POST, [] {
    if (server.header("X-Oni-Token") != controlToken) {
      server.send(403, "text/plain", "Invalid control token");
      return;
    }
    if (diagnosticBusy() || !radioOK || !frameRing) {
      server.send(409, "text/plain",
                  "Radio unavailable, buffers unavailable, or another mode is active");
      return;
    }
    uint8_t nextAddress[5];
    memcpy(nextAddress, captureAddress, 5);
    String a = server.arg("address");
    if (a.length() && !oni::parseAddress(a.c_str(), nextAddress, 5)) {
      server.send(400, "text/plain", "address must be 10 hex characters");
      return;
    }
    memcpy(captureAddress, nextAddress, 5);
    captureEnabled = true;
    pauseObservations();
    radio.stopListening();
    radio.setAutoAck(false);
    radio.setCRCLength(RF24_CRC_16);
    radio.disableDynamicPayloads();
    radio.setPayloadSize(32);
    radio.setAddressWidth(5);
    radio.setDataRate(RF24_1MBPS);
    radio.setChannel(CAPTURE_CHANNEL);
    for (int i = 0; i < 6; ++i)
      radio.closeReadingPipe(i);
    radio.flush_rx();
    radio.openReadingPipe(1, captureAddress);
    radio.startListening();
    server.send(200, "application/json",
                "{\"capture\":true,\"channel\":40,\"mode\":\"decoded nRF24 "
                "payloads\",\"crc\":\"hardware accepted frames\"}");
  });
  server.on("/capture/stop", HTTP_POST, [] {
    if (server.header("X-Oni-Token") != controlToken) {
      server.send(403, "text/plain", "Invalid control token");
      return;
    }
    if (!captureEnabled) {
      server.send(409, "text/plain", "Capture is not active");
      return;
    }
    captureEnabled = false;
    if (radioOK)
      monitorMode();
    resumeObservations();
    server.send(200, "application/json", "{\"capture\":false}");
  });
  server.on("/mqtt", HTTP_POST, [] {
#if ONI_HAS_MQTT
    if (server.header("X-Oni-Token") != controlToken) {
      server.send(403, "text/plain", "Invalid control token");
      return;
    }
    if (diagnosticBusy()) {
      server.send(409, "text/plain", "Stop diagnostics first");
      return;
    }
    mqttHost = server.arg("host");
    if (mqttHost.length() > 127) {
      server.send(400, "text/plain", "Broker host too long");
      return;
    }
    uint32_t port = server.arg("port").toInt();
    if (port >= 1 && port <= 65535)
      mqttPort = port;
    mqttTopic = server.arg("topic");
    if (mqttTopic.length() < 1 || mqttTopic.length() > 96)
      mqttTopic = "flawless/telemetry";
    mqttEnabled = server.arg("enabled") == "true";
    if (mqttEnabled && (!mqttHost.length() || !setupMqttWorker())) {
      mqttEnabled = false;
      server.send(503, "text/plain", "Broker host or worker unavailable");
      return;
    }
    nextMqtt = 0;
    server.send(200, "application/json",
                String("{\"enabled\":") + (mqttEnabled ? "true" : "false") +
                    ",\"topic\":" + quoted(mqttTopic) + "}");
#else
  server.send(501,"text/plain","MQTT support requires PubSubClient");
#endif
  });
  server.on("/udp", HTTP_POST, [] {
    if (server.header("X-Oni-Token") != controlToken) {
      server.send(403, "text/plain", "Invalid control token");
      return;
    }
    String e = server.arg("enabled"), p = server.arg("period_ms");
    udpEnabled = e == "true";
    if (p.length()) {
      uint32_t n = p.toInt();
      if (n >= 200 && n <= 10000)
        udpIntervalMs = n;
    }
    server.send(200, "application/json",
                String("{\"enabled\":") + (udpEnabled ? "true" : "false") + ",\"port\":39001}");
  });
  server.on("/ghost", HTTP_POST, [] {
    if (server.header("X-Oni-Token") != controlToken) {
      server.send(403, "text/plain", "Invalid control token");
      return;
    }
    if (diagnosticBusy()) {
      server.send(409, "text/plain", "Stop diagnostics first");
      return;
    }
    ghostEnabled = server.arg("enabled") == "true";
    server.send(200, "application/json",
                String("{\"enabled\":") + (ghostEnabled ? "true" : "false") +
                    ",\"mode\":\"periodic light sleep\"}");
  });
  server.on("/field", HTTP_GET, [] {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", fieldsJson());
  });
  server.on("/field/add", HTTP_POST, [] {
    if (server.header("X-Oni-Token") != controlToken) {
      server.send(403, "text/plain", "Invalid control token");
      return;
    }
    String marker = server.arg("marker");
    if (marker.length() < 1 || marker.length() > 48) {
      server.send(400, "text/plain", "marker 1..48 required");
      return;
    }
    int v = server.arg("rssi").toInt();
    fieldAdd(marker, (int16_t)constrain(v, -127, 0));
    server.send(200, "application/json", fieldsJson());
  });
  server.on("/export/all", HTTP_GET, [] {
    server.sendHeader("Content-Disposition", "attachment; filename=flawless-telemetry.json");
    server.send(200, "application/json",
                String("{\"intelligence\":") + intelligenceExportJson() +
                    ",\"events\":" + logsJson() + "}");
  });
  server.on("/export/csv", HTTP_GET, [] {
    server.sendHeader("Content-Disposition", "attachment; filename=flawless-frames.csv");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/csv", "");
    server.sendContent(csvHeader);
    for (uint16_t i = 0; i < frameCount; ++i)
      server.sendContent(frameCsvLine(
          frameRing[(frameHead + FRAME_RING_CAPACITY - frameCount + i) % FRAME_RING_CAPACITY]));
    server.sendContent("");
  });
  server.on("/snapshot/refresh", HTTP_POST, [] {
    if (server.header("X-Oni-Token") != controlToken) {
      server.send(403, "text/plain", "Invalid control token");
      return;
    }
#if ONI_HAS_MSC
    if (mscReady && !diagnosticBusy()) {
      snapshotRequested = true;
      server.send(202, "application/json", "{\"accepted\":true}");
    } else
      server.send(409, "text/plain", "Snapshot unavailable or diagnostic active");
#else
   server.send(501,"text/plain","USB-OTG MSC unavailable");
#endif
  });
  server.on("/logging", HTTP_POST, [] {
    if (server.header("X-Oni-Token") != controlToken) {
      server.send(403, "text/plain", "Invalid control token");
      return;
    }
    if (!fsOK) {
      server.send(503, "text/plain", "Filesystem unavailable");
      return;
    }
    telemetryLogging = server.arg("enabled") == "true";
    nextJournal = 0;
    server.send(200, "application/json",
                String("{\"enabled\":") + (telemetryLogging ? "true" : "false") + "}");
  });
  server.on("/journal", HTTP_GET, [] {
    File file = LittleFS.open("/telemetry.json", "r");
    if (!file) {
      server.send(404, "text/plain", "No saved journal");
      return;
    }
    server.sendHeader("Content-Disposition", "attachment; filename=flawless-journal.json");
    server.streamFile(file, "application/json");
    file.close();
  });

  setupDiagnosticApi();
}
