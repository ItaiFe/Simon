# Simon ↔ Flamingo Pairing + Ethernet — Design

Date: 2026-10-03
Builds on: `docs/superpowers/specs/2026-10-03-esp32-simon-design.md`
Repos touched: this repo (Simon) and `~/gits/Flamingods/esps/flamingo` (Flamingo firmware).
Reference for the existing protocol: `~/dev/Station` (`lib/station_core/Protocol.h`, `src/network.cpp`).

## Goal

Holding all four Simon buttons for 2 s pairs Simon with the Flamingo. Both flash pink, then a paired game starts during which the Flamingo's whole body mirrors exactly what Simon's strips show. A failure decouples (Flamingo returns to its normal behaviour); a win makes the Flamingo party, then decouples. Simon also gains Ethernet (WT32-ETH01's LAN8720) with WiFi as fallback.

Success criteria:
- Pairing works only via the 2 s all-four hold; single presses still start normal (unpaired) games.
- While paired, the Flamingo shows Simon's colours in sync and ignores the stations; stations behave exactly as today whenever Simon is not paired.
- Simon is fully playable if the Flamingo is off, unreachable, or running old firmware.
- Simon works on Ethernet or WiFi; OTA, `simon.local` and the Flamingo link work on both.

## Existing protocol (unchanged for stations)

Stations send UDP unicast to `flamingo-esp32.local:5000`, 2 bytes `[station_id 1..4, button_mask]` (bits: red 0x01, green 0x02, blue 0x04, yellow 0x08, white 0x10; 0x1F = special). `0` releases. The Flamingo blends all live station masks and fills its 3 strips with one colour. There is no handshake, no ack, no sender check.

## Simon protocol (station ID 5)

Simon sends `[5, value]` to the same host/port:

| Value | Flamingo action |
|---|---|
| `0x01` `0x02` `0x04` `0x08` | Fill all strips instantly (no easing) with red / green / blue / yellow from the Flamingo's palette |
| `0x00` | Fill black — Simon still paired (unlike stations, `0` does **not** release) |
| `0x20` | Fill pink (pairing flash) |
| `0xFE` | Win: start PARTY plan; when PARTY ends → IDLE. Simon is unpaired. |
| `0xFF` | Unpair: return to IDLE immediately |
| anything else | Ignored |

- **Paired** (Flamingo side) = SIMON plan active. Entered on any of `0x00`, `0x01`, `0x02`, `0x04`, `0x08`, `0x20` from ID 5. Left on `0xFF`, `0xFE`, or 2 s without any ID-5 packet.
- **Keepalive:** Simon sends its current value on every change and re-sends it every 100 ms while paired.
- **Reliability:** `0xFF` and `0xFE` are sent 3 times (like the stations' release).
- **No ack:** Simon never waits for the Flamingo.

## Simon side

### Pairing gesture and idle change
- **Normal games now start on release**, not press: in IDLE a press arms a start; when all buttons are released (and no pairing happened) a normal game starts. Single presses feel the same.
- **Hold all four buttons:** while all four are held in IDLE, strips fill with pink over `PAIR_HOLD_MS` (2000 ms), proportional to hold time. Releasing any button before 2 s cancels: strips return to idle, no game starts.
- **At 2 s:** Simon enters PAIR_FLASH: all strips flash pink (`PAIR_FLASH_MS`, ~800 ms) and Simon starts sending `0x20` to the Flamingo. After the flash and once all buttons are released, the paired game starts (GET_READY → SHOWING …) with mirroring on.
- Requires `Buttons` to expose current debounced held state: `bool held(uint8_t i) const`.

### Mirroring while paired
Simon sends the value matching what its own strips show:

| Simon state / frame | Value |
|---|---|
| PAIR_FLASH | `0x20` |
| GET_READY flourish, lead-in, gaps, waiting for input | `0x00` |
| Sequence step pulse (colour c) | bit for c |
| Player press pulse (colour c) | bit for c |
| ROUND_CLEARED green pulse | `0x02` |
| GAME_OVER red flash on / off | `0x01` / `0x00`; when the animation ends → `0xFF` ×3, unpaired |
| VICTORY | `0xFE` ×3 at the start of VICTORY, unpaired |

After GAME_OVER or VICTORY Simon returns to IDLE unpaired; the next game is normal unless paired again.

### Units
- `lib/FlamingoLink/` — pure C++, unit-tested: `FlamingoLink` holds paired flag, current value, timers; `update(now, value)` returns whether a packet must be sent now (on change or keepalive due); `endWithUnpair()/endWithWin()` queue 3 repeats; exposes the bytes to send. No Arduino deps.
- `src/Flamingo.{h,cpp}` — UDP socket, resolves `FLAMINGO_HOST` (mDNS name or IP literal) when the network is up, retries resolution every 5 s, sends packets produced by `FlamingoLink`. Logs `Flamingo resolved <ip>` / `Flamingo unreachable`.
- `src/main.cpp` — new states `PairHold`, `PairFlash`; `paired` flag; each frame computes the mirror value from the state/animation and feeds `FlamingoLink`.
- `Animations` — `pairHold(fraction)`, `pairFlash(t)`.

### Network: Ethernet first, WiFi fallback (`src/Net.cpp`)
- Start ETH (`ETH.begin` with LAN8720, PHY addr 1, MDC 23, MDIO 18, power 16, clock `ETH_CLOCK_GPIO0_IN`) and WiFi STA together at boot.
- When ETH gets an IP → WiFi off, log `Ethernet up <ip>, WiFi off`. When ETH link drops → WiFi back on, log `Ethernet down, WiFi fallback`.
- mDNS (`simon`) and ArduinoOTA start once any interface has an IP; ArduinoOTA listens on all interfaces.
- Pin check: ETH uses GPIO 0, 16, 18, 19, 21, 22, 23, 25, 26, 27 — no overlap with buttons (12, 15, 2, 5) or strips (4, 14, 32, 33). GPIO0 still works for flash mode (oscillator is off until firmware enables GPIO16).

### `config.h` additions
`FLAMINGO_HOST` ("flamingo-esp32.local"), `FLAMINGO_PORT` (5000), `FLAMINGO_SIMON_ID` (5), `FLAMINGO_KEEPALIVE_MS` (100), `FLAMINGO_REPEATS` (3), `FLAMINGO_RESOLVE_RETRY_MS` (5000), `PAIR_HOLD_MS` (2000), `PAIR_FLASH_MS` (800), `PAIR_COLOR` (pink, 0xFF1493), `ETH_ENABLED` (true).

## Flamingo side (`~/gits/Flamingods/esps/flamingo`)

- In `handleUDP()`: packets with `stationId == 5` go to new `handleSimon(value)`; stations (1–4) path unchanged; `MAX_STATIONS` stays 4.
- New plan `SIMON`: `handleSimon` sets the target colour; render fills all 3 strips immediately (no 80/20 easing). Colours from the existing palette (red, green, blue, yellow as used for stations); `0x20` = pink; `0x00` = black. Colour order (RBG) handled as today.
- While `SIMON` is active, station packets are still recorded but do not change the plan, the LEDs, or trigger special/mega-special modes.
- `0xFF` → IDLE. `0xFE` → PARTY (existing), then IDLE as PARTY already does. Timeout 2 s without ID-5 packets → IDLE.
- `handleUDP()` drains all pending packets each loop (currently one packet per loop with a 20 ms delay).
- Work happens on a branch in the Flamingods repo; build is verified; flashing the Flamingo is left to the user.

## Error handling
- Flamingo unresolved / no network: paired game plays on Simon only; packets are skipped; logged.
- Old Flamingo firmware: ignores ID 5 (fails its 1..4 check) — no effect on stations.
- Simon powers off mid-game: Flamingo returns to IDLE after 2 s.
- Lost unpair packets: covered by ×3 repeats and the 2 s timeout.

## Testing
- Native tests for `FlamingoLink`: send on change, keepalive at 100 ms, no sends when unpaired, unpair/win produce exactly 3 packets then stop, value encoding.
- Existing 16 SimonGame tests stay green.
- `pio run -e esp32dev` / `-e ota` build clean; Flamingo firmware builds clean.
- On hardware (user): pairing hold + pink flash on both, mirroring in sync, game over decouples, win → party, single press still starts a normal game, Ethernet ↔ WiFi failover, OTA over Ethernet.

## Out of scope
- Acks/handshake, exclusive ownership tokens, arbitrary RGB, Simon reading Flamingo state, changes to the Station firmware.
