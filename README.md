# Simon (ESP32)

Simon on an ESP32 with 4 buttons and 4 WS2812B LED strips. Flash once over USB, then over WiFi forever after.

## Wiring

| Color  | Button GPIO (other leg → GND) | Strip data GPIO |
|--------|-------------------------------|-----------------|
| Red    | IO12 | IO4 |
| Green  | IO15 | IO14 |
| Blue   | IO2 | IO32 (labeled **CFG**) |
| Yellow | IO5 (labeled **RXD2**) | IO33 (labeled **485_EN**) |

Board: WT32-ETH01. Buttons need no resistors (internal pull-ups). Strips: 5 V and GND from a 5 V supply; tie supply GND to ESP32 GND. If a strip flickers, add a 330 Ω resistor in series on its data line.

Leave IO0, TXD and RXD free for flashing (FTDI TX → RXD, FTDI RX → TXD, GND → GND; IO0 → GND during the first flash).

## Setup

1. Install PlatformIO (`pip install platformio` or the VS Code extension).
2. `cp secrets.example.ini secrets.ini` and fill in your WiFi and an OTA password. (`secrets.ini` is git-ignored.) Leave `wifi_ssid` empty to run without WiFi.

## Flashing

- **First time (USB):** `pio run -e esp32dev -t upload`
- **After that (WiFi):** `pio run -e ota -t upload`
  - If `simon.local` doesn't resolve: `pio run -e ota -t upload --upload-port <device IP>` (the IP is printed on serial at boot).
  - While uploading, all strips fill cyan like a progress bar; green = done (it reboots), red = failed.
- **Serial log:** `pio device monitor` — shows every state change, round, press and timeout.
- **Unit tests (game rules, on your computer):** `pio test -e native`

## How to play

Boot animation → idle glow. Press any button to start. Watch the sequence, repeat it. Round 1 is 3 colors; each round adds one and plays faster, up to 10. A wrong press or taking too long ends the game (red flash, then the button you should have pressed blinks). Clear round 8 for the victory show.

## Flamingo pairing

Hold **all 4 buttons for 2 seconds** while Simon is idle. The strips fill with pink as you hold; at 2 s Simon and the Flamingo flash pink together. Release the buttons and the game starts — the Flamingo's whole body now mirrors Simon's colours.

- A **wrong press / timeout** ends the game with 4 red flashes (on the Flamingo too) and unpairs: the Flamingo goes back to its rainbow and the stations work again.
- A **win** makes the Flamingo party for 10 s, then it unpairs.
- While paired, the stations are ignored. If Simon loses power, the Flamingo unpairs by itself after 2 s.
- Simon talks to `flamingo-esp32.local` on UDP 5000 as station ID 5 (`FLAMINGO_*` in `config.h`). The Flamingo needs its `simon-pairing` firmware (Flamingods repo). If the Flamingo is off or unreachable, Simon still plays normally.
- A normal game starts when you **release** a button.

## Network

Simon prefers **Ethernet** (plug a cable into the board): WiFi turns off while the cable is up and comes back automatically if it's unplugged. OTA uploads, `simon.local` and the Flamingo link work on either. Disable Ethernet with `ETH_ENABLED = false`.

## Tuning

Everything is in **`include/config.h`** — edit, then `pio run -e ota -t upload` (~30 s).

| Want to change… | Edit |
|---|---|
| Overall brightness / power draw | `MAX_BRIGHTNESS`, `POWER_LIMIT_MA` |
| Strip colors (e.g. yellow looks green) | `STRIP_COLORS` |
| Red/green swapped on a strip | `COLOR_ORDER` (`GRB` ↔ `RGB`) |
| Number of LEDs | `LEDS_PER_STRIP` |
| Rounds / sequence length | `START_SEQUENCE_LEN`, `MAX_SEQUENCE_LEN` |
| Game speed | `ON_MS_FIRST/LAST`, `GAP_MS_FIRST/LAST` |
| Time allowed per press | `TIMEOUT_MS_FIRST/LAST` |
| Press feedback length | `PRESS_PULSE_MS` |
| Double presses / missed presses | `DEBOUNCE_MS` |
| Idle look | `IDLE_*` |
| Boot / victory / game-over animations | `BOOT_*`, `VICTORY_*`, `GAME_OVER_*` |
| Round-cleared pulse | `ROUND_CLEARED_*` |
| Pause / flourish before playback | `SHOW_LEAD_IN_MS`, `GET_READY_MS` |

## First power-on checklist

- [ ] Boot: each strip fills LED by LED in red, green, blue, yellow; then all go white — every LED lit?
- [ ] Idle: four strips gently breathe in their colors, glow rotating around, occasional sparkles.
- [ ] Each button lights its own strip (serial shows the right color name).
- [ ] Full game: 3 colors first, +1 each round, visibly faster; round 8 → victory animation → idle.
- [ ] Wrong press → red flashes, then the correct strip blinks 3 times → idle.
- [ ] Wait without pressing → timeout game over (5 s in round 1).
- [ ] Mash buttons during playback, the green round-cleared pulse and game-over: none of them count; input always starts fresh.
- [ ] Press two buttons at the same instant during input: serial shows two `Press …` lines (lowest color first).
- [ ] Router off (or `wifi_ssid` empty): device still boots straight into the animation and plays; serial shows WiFi retry messages.
- [ ] `pio run -e ota -t upload` works with the USB cable unplugged.
- [ ] Pairing: hold all 4 buttons 2 s → pink fill, then Simon and Flamingo flash pink; the game shows on the Flamingo in sync.
- [ ] Holding only 3 buttons, or letting go before 2 s, does nothing (no pairing, no game).
- [ ] Wrong press while paired → red flashes on both, then the Flamingo returns to its rainbow; stations work again.
- [ ] Winning while paired → Flamingo party, then rainbow.
- [ ] Press a station during a paired game → the Flamingo keeps showing Simon only.
- [ ] Power Simon off mid-game while paired → Flamingo returns to rainbow within ~2 s.
- [ ] Flamingo switched off → pairing still flashes pink on Simon and the game plays normally without stutter.
- [ ] Ethernet cable in → serial `Ethernet up …, WiFi off`; pull it → `Ethernet down, WiFi fallback`; OTA works both ways.
