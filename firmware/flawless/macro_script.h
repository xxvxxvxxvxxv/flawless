#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
enum OniMacroActionType : uint8_t {
  ONI_MACRO_TEXT = 0,
  ONI_MACRO_ENTER,
  ONI_MACRO_TAB,
  ONI_MACRO_ESC,
  ONI_MACRO_BACKSPACE,
  ONI_MACRO_CTRL,
  ONI_MACRO_DELAY
};

struct OniMacroAction {
  OniMacroActionType type = ONI_MACRO_DELAY;
  char key = 0;
  uint16_t delayMs = 0;
  char text[128] = {};
};

namespace flawless {
inline bool parseMacro(const char *script, size_t length, OniMacroAction *out, size_t capacity,
                       uint8_t &count) {
  count = 0;
  if (!script || !length || length > 2048)
    return false;
  size_t start = 0;
  uint8_t parsed = 0;
  while (start < length) {
    size_t end = start;
    while (end < length && script[end] != '\n')
      ++end;
    size_t n = end - start;
    if (n && script[start + n - 1] == '\r')
      --n;
    const char *line = script + start;
    start = end + 1;
    if (!n || line[0] == '#')
      continue;
    if (parsed >= capacity || parsed == 255)
      return false;
    OniMacroAction action{};
    if (n >= 5 && !memcmp(line, "TEXT ", 5)) {
      action.type = ONI_MACRO_TEXT;
      size_t size = n - 5;
      if (size >= sizeof(action.text))
        return false;
      for (size_t i = 0; i < size; ++i)
        if ((unsigned char)line[i + 5] < 32 || (unsigned char)line[i + 5] > 126)
          return false;
      memcpy(action.text, line + 5, size);
    } else if (n == 5 && !memcmp(line, "ENTER", 5))
      action.type = ONI_MACRO_ENTER;
    else if (n == 3 && !memcmp(line, "TAB", 3))
      action.type = ONI_MACRO_TAB;
    else if (n == 3 && !memcmp(line, "ESC", 3))
      action.type = ONI_MACRO_ESC;
    else if (n == 9 && !memcmp(line, "BACKSPACE", 9))
      action.type = ONI_MACRO_BACKSPACE;
    else if (n == 6 && !memcmp(line, "CTRL ", 5)) {
      char key = line[5];
      if (key >= 'A' && key <= 'Z')
        key += 32;
      if (key < 'a' || key > 'z')
        return false;
      action.type = ONI_MACRO_CTRL;
      action.key = key;
    } else if (n > 6 && n <= 10 && !memcmp(line, "DELAY ", 6)) {
      uint32_t value = 0;
      for (size_t i = 6; i < n; ++i) {
        if (line[i] < '0' || line[i] > '9')
          return false;
        value = value * 10 + line[i] - '0';
      }
      if (value < 10 || value > 2000)
        return false;
      action.type = ONI_MACRO_DELAY;
      action.delayMs = uint16_t(value);
    } else
      return false;
    out[parsed++] = action;
  }
  count = parsed;
  return count > 0;
}
} // namespace flawless
