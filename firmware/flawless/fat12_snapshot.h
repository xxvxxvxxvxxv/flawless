#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace flawless {
// 128 KiB FAT12 superfloppy, one FAT, 32 root entries, 252 data clusters.
struct Fat12Snapshot {
  static constexpr size_t SIZE = 256 * 512;
  uint8_t *image;
  uint16_t next = 2;
  uint8_t entries = 0;
  explicit Fat12Snapshot(uint8_t *bytes) : image(bytes) {}
  static void word(uint8_t *p, uint16_t v) {
    p[0] = uint8_t(v);
    p[1] = uint8_t(v >> 8);
  }
  static void dword(uint8_t *p, uint32_t v) {
    word(p, uint16_t(v));
    word(p + 2, uint16_t(v >> 16));
  }
  void fat(uint16_t c, uint16_t value) {
    const size_t i = 512 + c + c / 2;
    if (c & 1) {
      image[i] = (image[i] & 15) | uint8_t(value << 4);
      image[i + 1] = uint8_t(value >> 4);
    } else {
      image[i] = uint8_t(value);
      image[i + 1] = (image[i + 1] & 240) | uint8_t((value >> 8) & 15);
    }
  }
  void begin() {
    memset(image, 0, SIZE);
    next = 2;
    entries = 0;
    image[0] = 0xeb;
    image[1] = 0x3c;
    image[2] = 0x90;
    memcpy(image + 3, "FLAWLESS", 8);
    word(image + 11, 512);
    image[13] = 1;
    word(image + 14, 1);
    image[16] = 1;
    word(image + 17, 32);
    word(image + 19, 256);
    image[21] = 0xf8;
    word(image + 22, 1);
    word(image + 24, 1);
    word(image + 26, 1);
    image[36] = 0x80;
    image[38] = 0x29;
    dword(image + 39, 0x464c4157);
    memcpy(image + 43, "FLAWLESS   ", 11);
    memcpy(image + 54, "FAT12   ", 8);
    image[510] = 0x55;
    image[511] = 0xaa;
    fat(0, 0xff8);
    fat(1, 0xfff);
  }
  // All-or-nothing: never expose a truncated JSON file as a valid snapshot.
  bool add(const char *name11, const char *data, size_t length) {
    const size_t clusters = (length + 511) / 512;
    if (entries >= 32 || clusters > size_t(254 - next))
      return false;
    uint8_t *e = image + 1024 + 32 * entries++;
    memcpy(e, name11, 11);
    e[11] = 0x21;
    word(e + 26, length ? next : 0);
    dword(e + 28, uint32_t(length));
    // 2026-01-01 FAT creation/modified date.
    word(e + 16, 0x5c21);
    word(e + 18, 0x5c21);
    word(e + 24, 0x5c21);
    for (size_t i = 0; i < clusters; ++i) {
      uint16_t c = next++;
      fat(c, i + 1 < clusters ? uint16_t(c + 1) : 0xfff);
      const size_t offset = i * 512, take = length - offset < 512 ? length - offset : 512;
      memcpy(image + 2048 + (c - 2) * 512, data + offset, take);
    }
    return true;
  }
};
} // namespace flawless
