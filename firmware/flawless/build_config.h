#pragma once
#include <soc/soc_caps.h>
#include <sdkconfig.h>
#define FLAWLESS_VERSION "1.0.0-rc1"
// Set to 0 only for an ESP32-only build. Wi-Fi, BLE and USB remain available.
#ifndef FLAWLESS_ENABLE_NRF
#define FLAWLESS_ENABLE_NRF 1
#endif
#if SOC_USB_OTG_SUPPORTED && defined(ARDUINO_USB_MODE) && ARDUINO_USB_MODE == 0
#define FLAWLESS_USB_OTG 1
#else
#define FLAWLESS_USB_OTG 0
#endif

// Explicit switches let Arduino discover the corresponding libraries.
// __has_include alone can silently omit a library before discovery runs.
#ifndef FLAWLESS_ENABLE_BLE
#define FLAWLESS_ENABLE_BLE 1
#endif
#ifndef FLAWLESS_ENABLE_WS
#define FLAWLESS_ENABLE_WS 1
#endif
#ifndef FLAWLESS_ENABLE_MQTT
#define FLAWLESS_ENABLE_MQTT 1
#endif
