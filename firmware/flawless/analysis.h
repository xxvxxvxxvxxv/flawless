#pragma once
#include <stdint.h>
struct Detector {
  uint8_t previous[126] = {};
  uint64_t since[126] = {};
  bool latched[126] = {};
  uint32_t frames = 0;
  template<class Emit> void update(const uint8_t* values, uint64_t now, Emit emit) {
    for (int i=0; i<126; ++i) {
      if (frames >= 10 && values[i] >= 75 && previous[i] <= 25)
        emit("BURST", i, values[i]);
      if (values[i] >= 75) {
        if (!since[i]) since[i] = now;
        if (!latched[i] && now-since[i] >= 5000) {
          emit("SUSTAINED", i, values[i]); latched[i]=true;
        }
      } else { since[i]=0; latched[i]=false; }
      previous[i]=values[i];
    }
    ++frames;
  }
};
