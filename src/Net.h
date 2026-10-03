#pragma once
#include <stdint.h>

// WiFi + ArduinoOTA. Never blocks: the game runs whether or not WiFi is up.
namespace Net {
typedef void (*ProgressFn)(float fraction);
typedef void (*EndFn)(bool ok);

void begin(ProgressFn onProgress, EndFn onEnd);
void update(uint32_t now);
bool connected();  // true when any network interface has an IP
}  // namespace Net
