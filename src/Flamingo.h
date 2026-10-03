#pragma once
#include <FlamingoLink.h>
#include <stdint.h>

// Sends FlamingoLink packets to the Flamingo over UDP. Never waits for a reply.
namespace Flamingo {
FlamingoLink& link();
void refresh();  // request a fresh host lookup on the next update()
// Call once per frame. Lookups block up to FLAMINGO_RESOLVE_TIMEOUT_MS, so periodic retries
// only happen when allowLookup is true (idle); refresh() forces one regardless.
void update(uint32_t now, bool networkUp, bool allowLookup);
}  // namespace Flamingo
