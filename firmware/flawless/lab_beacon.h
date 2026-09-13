#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace oni {
// One cycle advertises EVERY configured SSID. The task waits between cycles.
inline uint32_t beaconPeriodMs(uint32_t hz) { return hz ? 1000 / hz : 1000; }
inline size_t buildBeacon(uint8_t *out, size_t capacity, const char *ssid, const uint8_t bssid[6],
                          uint8_t channel, uint32_t periodMs, uint64_t timestampUs) {
  const size_t n = strlen(ssid);
  const size_t length = 36 + 2 + n + 14 + 3 + 6;
  if (!n || n > 32 || capacity < length || channel < 1 || channel > 14 || !periodMs ||
      periodMs > 1000 || (bssid[0] & 1))
    return 0;
  memset(out, 0, length);
  out[0] = 0x80; // Beacon; driver supplies sequence and FCS.
  memset(out + 4, 0xff, 6);
  memcpy(out + 10, bssid, 6);
  memcpy(out + 16, bssid, 6);
  for (uint8_t i = 0; i < 8; ++i)
    out[24 + i] = uint8_t(timestampUs >> (8 * i));
  const uint16_t tu = uint16_t((periodMs * 1000 + 512) / 1024);
  out[32] = uint8_t(tu);
  out[33] = uint8_t(tu >> 8);
  out[34] = 0x01;
  out[35] = 0x00; // ESS, open network.
  size_t p = 36;
  out[p++] = 0;
  out[p++] = uint8_t(n);
  memcpy(out + p, ssid, n);
  p += n;
  const uint8_t rates[] = {0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24};
  out[p++] = 1;
  out[p++] = sizeof(rates);
  memcpy(out + p, rates, sizeof(rates));
  p += sizeof(rates);
  const uint8_t extendedRates[] = {0x30, 0x48};
  out[p++] = 50;
  out[p++] = sizeof(extendedRates);
  memcpy(out + p, extendedRates, sizeof(extendedRates));
  p += sizeof(extendedRates);
  out[p++] = 3;
  out[p++] = 1;
  out[p++] = channel;
  const uint8_t tim[] = {5, 4, 0, 1, 0, 0}; // Empty TIM, DTIM every beacon.
  memcpy(out + p, tim, sizeof(tim));
  return length;
}
} // namespace oni
