#include "Net.h"
#include <ArduinoOTA.h>
#include <ETH.h>
#include <WiFi.h>
#include "Log.h"
#include "config.h"

#if !defined(WIFI_SSID) || !defined(WIFI_PASSWORD) || !defined(OTA_PASSWORD)
#error "WiFi/OTA secrets missing: copy secrets.example.ini to secrets.ini and fill it in"
#endif

namespace {

bool wifiEnabled = false;
bool otaStarted = false;
volatile bool ethUp = false;  // written from the network event task
bool ethWasUp = false;
uint32_t lastRetry = 0;
Net::ProgressFn progressFn = nullptr;
Net::EndFn endFn = nullptr;

void onEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      ETH.setHostname(HOSTNAME);
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      ethUp = true;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
    case ARDUINO_EVENT_ETH_STOP:
      ethUp = false;
      break;
    default:
      break;
  }
}

void startWifi() {
  WiFi.setHostname(HOSTNAME);  // must precede WiFi.mode() on core 2.x
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);  // modem sleep adds latency that makes OTA uploads drop
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  LOG("WiFi connecting to %s", WIFI_SSID);
}

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
  LOG("OTA ready at %s.local (%s via %s)", HOSTNAME,
      ethUp ? ETH.localIP().toString().c_str() : WiFi.localIP().toString().c_str(),
      ethUp ? "Ethernet" : "WiFi");
}

}  // namespace

void Net::begin(ProgressFn onProgress, EndFn onEnd) {
  progressFn = onProgress;
  endFn = onEnd;
  WiFi.onEvent(onEvent);
  if (ETH_ENABLED) {
    // Same bring-up as the Flamingo on this board: power the PHY, then start the MAC.
    pinMode(ETH01_PHY_POWER_PIN, OUTPUT);
    digitalWrite(ETH01_PHY_POWER_PIN, HIGH);
    if (ETH.begin(ETH01_PHY_ADDR, ETH01_PHY_POWER_PIN, ETH01_MDC_PIN, ETH01_MDIO_PIN,
                  ETH_PHY_LAN8720, ETH_CLOCK_GPIO0_IN)) {
      LOG("Ethernet starting");
    } else {
      LOG("Ethernet init failed");
    }
  }
  wifiEnabled = strlen(WIFI_SSID) > 0;
  if (wifiEnabled) startWifi();
  else LOG("WiFi disabled (wifi_ssid empty in secrets.ini)");
}

bool Net::connected() { return ethUp || WiFi.status() == WL_CONNECTED; }

void Net::update(uint32_t now) {
  const bool eth = ethUp;
  if (eth != ethWasUp) {
    ethWasUp = eth;
    if (eth) {
      LOG("Ethernet up %s, WiFi off", ETH.localIP().toString().c_str());
      if (WiFi.getMode() != WIFI_OFF) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
      }
    } else {
      LOG("Ethernet down%s", wifiEnabled ? ", WiFi fallback" : "");
      if (wifiEnabled) startWifi();
    }
  }

  if (!connected()) {
    if (wifiEnabled && !eth && now - lastRetry >= WIFI_RETRY_MS) {
      lastRetry = now;
      LOG("WiFi not connected, retrying");
      WiFi.reconnect();
    }
    return;
  }
  if (!otaStarted) startOta();
  ArduinoOTA.handle();
}
