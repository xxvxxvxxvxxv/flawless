#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Arduino_GFX_Library.h>

constexpr uint8_t NRF_MOSI = 11;
constexpr uint8_t NRF_SCK = 12;
constexpr uint8_t NRF_MISO = 13;
constexpr uint8_t NRF_CSN = 14;
constexpr uint8_t NRF_CE = 15;
constexpr uint8_t BOOT_BUTTON = 0;
constexpr uint8_t CHANNELS = 126;
constexpr uint8_t SAMPLES = 12;
constexpr uint16_t DWELL_US = 140;
constexpr uint16_t GRAPH_X = 42;
constexpr uint16_t GRAPH_Y = 52;
constexpr uint16_t GRAPH_W = 478;
constexpr uint16_t GRAPH_H = 124;
constexpr uint8_t WATERFALL_ROWS = 36;

Arduino_DataBus *displayBus = new Arduino_ESP32QSPI(6, 47, 18, 7, 48, 5);
Arduino_GFX *display = new Arduino_RM67162(displayBus, 17, 0);
SPIClass radioBus(HSPI);
RF24 radio(NRF_CE, NRF_CSN);
WebServer server(80);

uint8_t occupancy[CHANNELS];
uint8_t smoothed[CHANNELS];
uint8_t waterfall[WATERFALL_ROWS][CHANNELS];
uint32_t frameNumber = 0;
uint32_t lastWiFiScan = 0;
uint32_t lastButtonChange = 0;
bool lastButtonState = HIGH;
uint8_t page = 0;
int networkCount = 0;
String networkNames[10];
int32_t networkRssi[10];
int32_t networkChannels[10];

const char dashboard[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>RF Sentinel S3</title><style>*{box-sizing:border-box}body{margin:0;background:#05080d;color:#e9f6ff;font-family:Inter,ui-sans-serif,system-ui}main{max-width:1100px;margin:auto;padding:28px}.top{display:flex;justify-content:space-between;align-items:center;margin-bottom:22px}.brand{letter-spacing:.18em;font-weight:800}.live{color:#52ffd0;font-size:13px}.card{background:linear-gradient(145deg,#0b1420,#070c13);border:1px solid #1b3041;border-radius:20px;padding:18px;box-shadow:0 20px 60px #0008}canvas{width:100%;height:420px}.stats{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;margin-top:14px}.stat{background:#08111b;border:1px solid #152b3d;border-radius:14px;padding:14px}.label{color:#7891a4;font-size:12px;text-transform:uppercase;letter-spacing:.12em}.value{font-size:24px;font-weight:700;margin-top:4px}@media(max-width:650px){main{padding:14px}.stats{grid-template-columns:1fr}canvas{height:300px}}</style></head><body><main><div class="top"><div class="brand">RF SENTINEL S3</div><div class="live">● LIVE / RECEIVE ONLY</div></div><div class="card"><canvas id="plot"></canvas><div class="stats"><div class="stat"><div class="label">Peak frequency</div><div class="value" id="peak">--</div></div><div class="stat"><div class="label">Peak occupancy</div><div class="value" id="level">--</div></div><div class="stat"><div class="label">Scan frame</div><div class="value" id="frame">--</div></div></div></div></main><script>const c=document.querySelector('#plot'),x=c.getContext('2d');function size(){c.width=c.clientWidth*devicePixelRatio;c.height=c.clientHeight*devicePixelRatio}addEventListener('resize',size);size();function color(v){if(v>70)return'#ff4365';if(v>35)return'#ffcc4a';return'#35e4ff'}async function tick(){try{const r=await fetch('/data',{cache:'no-store'}),d=await r.json(),w=c.width,h=c.height,p=devicePixelRatio;x.fillStyle='#05080d';x.fillRect(0,0,w,h);x.strokeStyle='#142739';x.lineWidth=p;for(let i=0;i<6;i++){const y=h*i/5;x.beginPath();x.moveTo(0,y);x.lineTo(w,y);x.stroke()}const bw=w/d.values.length;d.values.forEach((v,i)=>{const bh=(h-28*p)*v/100;x.fillStyle=color(v);x.fillRect(i*bw,h-bh,bw*.72,bh)});x.fillStyle='#7891a4';x.font=`${11*p}px system-ui`;x.fillText('2400 MHz',4*p,14*p);x.fillText('2462',w/2-15*p,14*p);x.fillText('2525 MHz',w-62*p,14*p);document.querySelector('#peak').textContent=d.peakMHz+' MHz';document.querySelector('#level').textContent=d.peak+'%';document.querySelector('#frame').textContent=d.frame}catch(e){}setTimeout(tick,450)}tick()</script></body></html>
)HTML";

uint16_t heatColor(uint8_t value) {
  if (value > 75) return display->color565(255, 52, 88);
  if (value > 45) return display->color565(255, 190, 55);
  if (value > 18) return display->color565(35, 224, 255);
  return display->color565(18, 62, 78);
}

void scanSpectrum() {
  for (uint8_t channel = 0; channel < CHANNELS; channel++) {
    uint8_t hits = 0;
    radio.stopListening();
    radio.setChannel(channel);
    for (uint8_t sample = 0; sample < SAMPLES; sample++) {
      radio.startListening();
      delayMicroseconds(DWELL_US);
      if (radio.testRPD()) hits++;
      radio.stopListening();
    }
    occupancy[channel] = hits * 100 / SAMPLES;
    smoothed[channel] = (smoothed[channel] * 3 + occupancy[channel]) / 4;
  }

  for (int row = WATERFALL_ROWS - 1; row > 0; row--) {
    memcpy(waterfall[row], waterfall[row - 1], CHANNELS);
  }
  memcpy(waterfall[0], occupancy, CHANNELS);
  frameNumber++;
}

uint8_t peakChannel() {
  uint8_t best = 0;
  for (uint8_t channel = 1; channel < CHANNELS; channel++) {
    if (smoothed[channel] > smoothed[best]) best = channel;
  }
  return best;
}

void drawTab(uint16_t x, uint16_t width, const char *label, bool selected) {
  uint16_t border = display->color565(28, 62, 80);
  uint16_t active = display->color565(28, 112, 122);
  uint16_t textColor = selected ? display->color565(235, 255, 252) : display->color565(105, 139, 158);

  if (selected) {
    display->fillRoundRect(x, 9, width, 27, 6, active);
  } else {
    display->drawRoundRect(x, 9, width, 27, 6, border);
  }

  display->setTextColor(textColor);
  display->setTextSize(1);
  display->setCursor(x + 10, 19);
  display->print(label);
}

void drawNavigation(uint8_t selected) {
  display->fillScreen(display->color565(5, 8, 13));
  drawTab(12, 102, "SPECTRUM", selected == 0);
  drawTab(120, 108, "WATERFALL", selected == 1);
  drawTab(234, 72, "WI-FI", selected == 2);
  display->setTextSize(1);
  display->setTextColor(display->color565(82, 255, 208));
  display->setCursor(430, 20);
  display->print("RX ONLY");
}

void renderSpectrum() {
  drawNavigation(0);
  uint16_t gridColor = display->color565(18, 42, 56);
  uint16_t axisColor = display->color565(91, 123, 143);

  display->setTextSize(1);
  display->setTextColor(axisColor);

  for (uint8_t level = 0; level <= 100; level += 25) {
    uint16_t y = GRAPH_Y + GRAPH_H - level * GRAPH_H / 100;
    display->drawLine(GRAPH_X, y, GRAPH_X + GRAPH_W, y, gridColor);
    display->setCursor(5, y > 3 ? y - 3 : y);
    display->printf("%3u", level);
  }

  for (uint16_t frequency = 2400; frequency <= 2525; frequency += 25) {
    uint16_t channel = frequency - 2400;
    uint16_t x = GRAPH_X + channel * GRAPH_W / 125;
    display->drawLine(x, GRAPH_Y, x, GRAPH_Y + GRAPH_H, gridColor);
    uint16_t labelX = x > 12 ? x - 12 : 0;
    if (labelX > 507) labelX = 507;
    display->setCursor(labelX, GRAPH_Y + GRAPH_H + 9);
    display->printf("%u", frequency);
  }

  display->setCursor(5, 42);
  display->print("ACT %");
  display->setCursor(494, GRAPH_Y + GRAPH_H + 20);
  display->print("MHz");
  display->drawRect(GRAPH_X - 1, GRAPH_Y - 1, GRAPH_W + 2, GRAPH_H + 2, display->color565(24, 48, 65));

  for (uint8_t channel = 0; channel < CHANNELS; channel++) {
    uint16_t x = GRAPH_X + channel * GRAPH_W / CHANNELS;
    uint16_t next = GRAPH_X + (channel + 1) * GRAPH_W / CHANNELS;
    uint16_t height = smoothed[channel] * GRAPH_H / 100;
    uint16_t barWidth = next > x + 1 ? next - x - 1 : 1;
    display->fillRect(x, GRAPH_Y + GRAPH_H - height, barWidth, height, heatColor(smoothed[channel]));
  }

  uint8_t peak = peakChannel();
  uint16_t peakX = GRAPH_X + peak * GRAPH_W / 125;
  display->drawLine(peakX, GRAPH_Y, peakX, GRAPH_Y + GRAPH_H, display->color565(255, 67, 101));
  display->setTextColor(display->color565(233, 246, 255));
  display->setTextSize(1);
  display->setCursor(12, 222);
  display->printf("PEAK  %u MHz  |  %u%%", 2400 + peak, smoothed[peak]);
  display->setTextColor(display->color565(117, 145, 164));
  display->setCursor(426, 222);
  display->printf("FRAME %lu", frameNumber);
}

void renderWaterfall() {
  drawNavigation(1);
  uint16_t top = 51;
  uint16_t left = 42;
  uint16_t width = 478;
  uint16_t height = 148;

  for (uint8_t row = 0; row < WATERFALL_ROWS; row++) {
    uint16_t y1 = top + row * height / WATERFALL_ROWS;
    uint16_t y2 = top + (row + 1) * height / WATERFALL_ROWS;
    for (uint8_t channel = 0; channel < CHANNELS; channel++) {
      uint16_t x1 = left + channel * width / CHANNELS;
      uint16_t x2 = left + (channel + 1) * width / CHANNELS;
      uint16_t cellWidth = x2 > x1 ? x2 - x1 : 1;
      uint16_t cellHeight = y2 > y1 ? y2 - y1 : 1;
      display->fillRect(x1, y1, cellWidth, cellHeight, heatColor(waterfall[row][channel]));
    }
  }

  display->drawRect(left - 1, top - 1, width + 2, height + 2, display->color565(24, 48, 65));
  display->setTextColor(display->color565(117, 145, 164));
  display->setTextSize(1);
  display->setCursor(5, 51);
  display->print("NOW");
  display->setCursor(5, 190);
  display->print("PAST");

  for (uint16_t frequency = 2400; frequency <= 2525; frequency += 25) {
    uint16_t channel = frequency - 2400;
    uint16_t x = left + channel * width / 125;
    uint16_t labelX = x > 12 ? x - 12 : 0;
    if (labelX > 507) labelX = 507;
    display->setCursor(labelX, 211);
    display->printf("%u", frequency);
  }

  display->setCursor(494, 224);
  display->print("MHz");
}

void renderNetworks() {
  drawNavigation(2);
  display->setTextSize(1);
  display->setTextColor(display->color565(117, 145, 164));
  display->setCursor(20, 48);
  display->print("NEARBY NETWORK");
  display->setCursor(320, 48);
  display->print("CHANNEL");
  display->setCursor(440, 48);
  display->print("SIGNAL");

  int rows = min(networkCount, 7);
  for (int i = 0; i < rows; i++) {
    int y = 68 + i * 21;
    String name = networkNames[i];
    if (name.length() > 28) name = name.substring(0, 28);
    display->setTextColor(display->color565(233, 246, 255));
    display->setCursor(20, y);
    display->print(name);
    display->setTextColor(display->color565(53, 228, 255));
    display->setCursor(320, y);
    display->printf("CH %2ld", networkChannels[i]);
    display->setCursor(420, y);
    display->printf("%4ld dBm", networkRssi[i]);
  }

  display->setTextColor(display->color565(117, 145, 164));
  display->setCursor(20, 224);
  display->printf("%d networks   AP: 192.168.4.1", networkCount);
}

void updateButton() {
  bool state = digitalRead(BOOT_BUTTON);
  if (state != lastButtonState && millis() - lastButtonChange > 40) {
    lastButtonChange = millis();
    lastButtonState = state;
    if (state == LOW) page = (page + 1) % 3;
  }
}

void updateWiFiScan() {
  if (millis() - lastWiFiScan < 15000 && lastWiFiScan != 0) return;
  int result = WiFi.scanComplete();
  if (result >= 0) {
    networkCount = min(result, 10);
    for (int i = 0; i < networkCount; i++) {
      networkNames[i] = WiFi.SSID(i);
      networkRssi[i] = WiFi.RSSI(i);
      networkChannels[i] = WiFi.channel(i);
    }
    WiFi.scanDelete();
    WiFi.scanNetworks(true, false, false, 120);
    lastWiFiScan = millis();
  } else if (result == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true, false, false, 120);
    lastWiFiScan = millis();
  }
}

String spectrumJson() {
  uint8_t peak = peakChannel();
  String json;
  json.reserve(700);
  json = "{\"frame\":" + String(frameNumber) + ",\"peakMHz\":" + String(2400 + peak) + ",\"peak\":" + String(smoothed[peak]) + ",\"values\":[";
  for (uint8_t channel = 0; channel < CHANNELS; channel++) {
    if (channel) json += ',';
    json += smoothed[channel];
  }
  json += "]}";
  return json;
}

void setup() {
  Serial.begin(115200);
  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  if (!display->begin(40000000)) {
    Serial.println("DISPLAY_INIT_FAILED");
    while (true) delay(1000);
  }
  display->setRotation(1);
  display->fillScreen(display->color565(0, 0, 0));

  radioBus.begin(NRF_SCK, NRF_MISO, NRF_MOSI, NRF_CSN);
  if (!radio.begin(&radioBus) || !radio.isChipConnected()) {
    display->setTextColor(display->color565(255, 67, 101));
    display->setTextSize(2);
    display->setCursor(24, 100);
    display->print("NRF24 NOT FOUND");
    Serial.println("NRF24_INIT_FAILED");
    while (true) delay(1000);
  }

  radio.setAutoAck(false);
  radio.disableCRC();
  radio.setAddressWidth(3);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_MIN);
  radio.startListening();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("RF-Sentinel-S3", "observe24", 1, false, 2);
  WiFi.scanNetworks(true, false, false, 120);

  server.on("/", []() { server.send_P(200, "text/html", dashboard); });
  server.on("/data", []() { server.send(200, "application/json", spectrumJson()); });
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();

  Serial.println("RF_SENTINEL_READY");
  Serial.println("DASHBOARD http://192.168.4.1");
}

void loop() {
  scanSpectrum();
  updateButton();
  updateWiFiScan();
  server.handleClient();

  if (page == 0) renderSpectrum();
  if (page == 1) renderWaterfall();
  if (page == 2) renderNetworks();

  server.handleClient();
}
