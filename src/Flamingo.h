#pragma once
#include <FlamingoLink.h>
#include <stdint.h>

// Sends FlamingoLink packets to the Flamingo over UDP. Never waits for a reply.
namespace Flamingo {
FlamingoLink& link();
void refresh();  // request a fresh host lookup on the next update()
// Call once per frame. Lookups block up to FLAMINGO_RESOLVE_TIMEOUT_MS, so they only run when
// allowLookup is true (idle / pairing flash); refresh() requests one at the next allowed frame.
void update(uint32_t now, bool networkUp, bool allowLookup);
}  // namespace Flamingo
