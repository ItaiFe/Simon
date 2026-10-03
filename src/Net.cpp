#include "Net.h"
#include <ArduinoOTA.h>
#include <WiFi.h>
#include "Log.h"
#include "config.h"

#if !defined(WIFI_SSID) || !defined(WIFI_PASSWORD) || !defined(OTA_PASSWORD)
#error "WiFi/OTA secrets missing: copy secrets.example.ini to secrets.ini and fill it in"
#endif

namespace {

bool enabled = false;
bool otaStarted = false;
uint32_t lastRetry = 0;
Net::ProgressFn progressFn = nullptr;
Net::EndFn endFn = nullptr;

void startOta() {
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    LOG("OTA start");
    progressFn(0);
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    progressFn(total ? (float)done / total : 0);
  });
  ArduinoOTA.onEnd([]() {
    LOG("OTA done, rebooting");
    endFn(true);
  });
  ArduinoOTA.onError([](ota_error_t err) {
    LOG("OTA error %u", (unsigned)err);
    endFn(false);
  });
  ArduinoOTA.begin();
  otaStarted = true;
  LOG("WiFi connected, OTA ready at %s.local (%s)", HOSTNAME, WiFi.localIP().toString().c_str());
}

}  // namespace

void Net::begin(ProgressFn onProgress, EndFn onEnd) {
  progressFn = onProgress;
  endFn = onEnd;
  if (strlen(WIFI_SSID) == 0) {
    LOG("WiFi disabled (wifi_ssid empty in secrets.ini)");
    return;
  }
  enabled = true;
  WiFi.setHostname(HOSTNAME);  // must precede WiFi.mode() on core 2.x
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);  // modem sleep adds latency that makes OTA uploads drop
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  LOG("WiFi connecting to %s", WIFI_SSID);
}

void Net::update(uint32_t now) {
  if (!enabled) return;
  if (WiFi.status() == WL_CONNECTED) {
    if (!otaStarted) startOta();
    ArduinoOTA.handle();
    return;
  }
  if (now - lastRetry >= WIFI_RETRY_MS) {
    lastRetry = now;
    LOG("WiFi not connected, retrying");
    WiFi.reconnect();
  }
}

bool Net::connected() { return WiFi.status() == WL_CONNECTED; }
