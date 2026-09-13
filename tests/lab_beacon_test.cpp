#include <cassert>
#include <cstdint>
#include <cstring>
#include "../firmware/flawless/lab_beacon.h"

int main() {
  const uint8_t bssid[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  uint8_t frame[128]{};
  const size_t length = oni::buildBeacon(frame, sizeof(frame), "ONI-LAB-ROOT", bssid, 1, 102, 123456);
  assert(length == 73);
  assert(frame[0] == 0x80 && frame[1] == 0x00);
  assert(frame[4] == 0xff && frame[9] == 0xff);
  assert(!std::memcmp(frame + 10, bssid, 6));
  assert(!std::memcmp(frame + 16, bssid, 6));
  assert(frame[32] == 0x64 && frame[33] == 0x00); // 100 TU, little-endian.
  assert(frame[34] == 0x01 && frame[35] == 0x00);
  assert(frame[36] == 0x00 && frame[37] == 12);
  assert(!std::memcmp(frame + 38, "ONI-LAB-ROOT", 12));
  // Supported Rates, Extended Supported Rates, DS Parameter Set, TIM.
  assert(frame[50] == 1 && frame[51] == 8);
  const uint8_t rates[]={0x82,0x84,0x8b,0x96,0x0c,0x12,0x18,0x24};
  assert(!memcmp(frame+52,rates,8));
  assert(frame[62]==0x30&&frame[63]==0x48);
  assert(frame[60] == 50 && frame[61] == 2);
  assert(frame[64] == 3 && frame[65] == 1 && frame[66] == 1);
  assert(frame[67] == 5 && frame[68] == 4);
  assert(oni::beaconPeriodMs(1) == 1000);
  assert(oni::beaconPeriodMs(10) == 100);
  assert(oni::buildBeacon(frame, sizeof(frame), "", bssid, 1, 100, 0) == 0);
  assert(oni::buildBeacon(frame, sizeof(frame), "bad", bssid, 0, 100, 0) == 0);
}
