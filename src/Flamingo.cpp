#include "Flamingo.h"
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "Log.h"
#include "config.h"

namespace {

FlamingoLinkConfig makeConfig() {
  FlamingoLinkConfig c;
  c.simonId = FLAMINGO_SIMON_ID;
  c.keepaliveMs = FLAMINGO_KEEPALIVE_MS;
  c.repeats = FLAMINGO_REPEATS;
  return c;
}

FlamingoLink linkInstance(makeConfig());
WiFiUDP udp;
IPAddress flamingoIp;
bool resolved = false;
bool lookupRequested = true;
bool hadNetwork = false;
uint32_t lastLookup = 0;

bool lookup() {
  const String host = FLAMINGO_HOST;
  IPAddress ip;
  if (ip.fromString(host)) {
    flamingoIp = ip;
    return true;
  }
  if (host.endsWith(".local")) {
    ip = MDNS.queryHost(host.substring(0, host.length() - 6), FLAMINGO_RESOLVE_TIMEOUT_MS);
    if (ip == IPAddress()) return false;  // 0.0.0.0 = not found
    flamingoIp = ip;
    return true;
  }
  if (WiFi.hostByName(host.c_str(), ip) != 1) return false;
  flamingoIp = ip;
  return true;
}

}  // namespace

FlamingoLink& Flamingo::link() { return linkInstance; }

void Flamingo::refresh() { lookupRequested = true; }

void Flamingo::update(uint32_t now, bool networkUp, bool allowLookup) {
  uint8_t packet[FlamingoLink::kPacketSize];
  if (!networkUp) {
    if (hadNetwork) lookupRequested = true;  // re-check later, but keep the last-known address
    hadNetwork = false;
    while (linkInstance.poll(now, packet)) {
    }  // nowhere to send: drop due packets so repeats/keepalive state stays current
    return;
  }
  hadNetwork = true;

  const bool retryDue = allowLookup && !resolved && now - lastLookup >= FLAMINGO_RESOLVE_RETRY_MS;
  // Lookups block, so they only ever run where the caller allows (never mid-game).
  if (allowLookup && (lookupRequested || retryDue)) {
    lookupRequested = false;
    lastLookup = now;
    if (lookup()) {
      resolved = true;
      LOG("Flamingo at %s", flamingoIp.toString().c_str());
    } else if (resolved) {
      LOG("Flamingo lookup missed, keeping %s", flamingoIp.toString().c_str());
    } else {
      LOG("Flamingo unreachable (%s)", FLAMINGO_HOST);
    }
  }

  if (!linkInstance.poll(now, packet) || !resolved) return;
  udp.beginPacket(flamingoIp, FLAMINGO_PORT);
  udp.write(packet, sizeof(packet));
  udp.endPacket();
}
