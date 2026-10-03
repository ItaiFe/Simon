#pragma once
#include <stdint.h>

// =====================================================================
//  Simon — every tunable setting lives in this file.
//  Edit a value, then re-flash over WiFi:   pio run -e ota -t upload
//  Strip/button order everywhere: 0 = red, 1 = green, 2 = blue, 3 = yellow
// =====================================================================

// ---------- Pins (ESP32 GPIO numbers) ----------
// Board: WT32-ETH01. Buttons go GPIO -> button -> GND (internal pull-ups, no resistors).
// Buttons sit on the boot-strapping pins (12, 15, 2, 5): a button can only pull them LOW,
// which is safe for booting and flashing. LED data uses plain output pins.
// Avoid GPIO 34-39 for buttons: input-only with no internal pull-up.
#define BUTTON_PIN_RED     12
#define BUTTON_PIN_GREEN   15
#define BUTTON_PIN_BLUE    2
#define BUTTON_PIN_YELLOW  5   // labeled RXD2 on the board

#define LED_PIN_RED        4
#define LED_PIN_GREEN      14
#define LED_PIN_BLUE       32  // labeled CFG on the board
#define LED_PIN_YELLOW     33  // labeled 485_EN on the board

// ---------- LED strips ----------
#define LED_TYPE           WS2812B
#define COLOR_ORDER        RGB          // these strips are RGB; use GRB if red and green look swapped
constexpr uint8_t  NUM_STRIPS      = 4;
constexpr uint8_t  LEDS_PER_STRIP  = 100;
constexpr uint8_t  MAX_BRIGHTNESS  = 160;   // global brightness cap, 0-255
constexpr uint32_t POWER_LIMIT_MA  = 1500;  // FastLED dims everything to stay under this (5 V)
// Strip colors as 0xRRGGBB. Green LEDs are very bright, so yellow uses less green than 0xFFFF00
// to stay distinct from the green strip. More green = yellower, less = more orange.
constexpr uint32_t STRIP_COLORS[NUM_STRIPS] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFA000};

// ---------- Buttons ----------
constexpr uint32_t DEBOUNCE_MS = 30;  // raise if a single press registers twice

// ---------- Game ----------
constexpr uint8_t  START_SEQUENCE_LEN = 3;   // colors in round 1
constexpr uint8_t  MAX_SEQUENCE_LEN   = 10;  // colors in final round (rounds = MAX - START + 1)
// Speed curve: value for round 1 and for the final round; rounds in between are interpolated.
constexpr uint16_t ON_MS_FIRST      = 600;   // how long each color lights during playback
constexpr uint16_t ON_MS_LAST       = 250;
constexpr uint16_t GAP_MS_FIRST     = 250;   // dark time between colors during playback
constexpr uint16_t GAP_MS_LAST      = 100;
constexpr uint16_t TIMEOUT_MS_FIRST = 5000;  // max wait for each press before game over
constexpr uint16_t TIMEOUT_MS_LAST  = 3000;
constexpr uint32_t PRESS_PULSE_MS   = 250;   // light shown when the player presses a button
constexpr uint32_t GET_READY_MS     = 600;   // flourish after the start press
constexpr uint32_t SHOW_LEAD_IN_MS  = 400;   // dark pause before each playback
constexpr uint8_t  PULSE_FADE_PERCENT = 30;  // last % of every pulse fades out (0 = hard off)

// ---------- Boot animation (~3 s) ----------
constexpr uint32_t BOOT_SWEEP_MS_PER_STRIP = 500;  // LED-by-LED fill of each strip
constexpr uint32_t BOOT_WHITE_MS           = 500;  // all strips white (spot dead pixels)
constexpr uint8_t  BOOT_WHITE_LEVEL        = 200;
constexpr uint32_t BOOT_FADE_MS            = 600;  // white fades into the idle animation

// ---------- Idle animation ----------
constexpr uint8_t  IDLE_MAX_BRIGHTNESS    = 140;    // 0-255, applied on top of MAX_BRIGHTNESS
constexpr float    IDLE_MIN_LEVEL         = 0.35f;  // dimmest point of the breath (0-1)
constexpr uint32_t IDLE_BREATH_PERIOD_MS  = 2000;   // one breath; strips are offset so it rotates
constexpr uint32_t IDLE_SHIMMER_PERIOD_MS = 700;    // wave travelling along each strip
constexpr float    IDLE_SHIMMER_SPACING   = 0.25f;  // wave phase between neighbouring LEDs (radians)
constexpr float    IDLE_SHIMMER_DEPTH     = 0.3f;   // 0 = no shimmer, 1 = full
// Comets: bright streaks chase each other up each strip in its color, without pause.
constexpr uint32_t IDLE_COMET_SPEED       = 65;     // LEDs per second
constexpr uint8_t  IDLE_COMET_TAIL        = 12;     // tail length in LEDs (0 = no comets)
constexpr uint8_t  IDLE_COMET_SPACING     = 30;     // distance between comets in LEDs (> tail)
constexpr uint8_t  IDLE_COMET_HEAD_WHITE  = 90;     // white added to the comet head (0-255)
// Confetti sparkles in random colors.
constexpr uint8_t  IDLE_SPARKLE_CHANCE    = 40;     // per strip per frame, out of 255
constexpr uint8_t  IDLE_SPARKLE_LEVEL     = 220;    // sparkle peak brightness
constexpr uint8_t  IDLE_SPARKLE_DECAY     = 8;      // sparkle fade per frame (lower = longer)
constexpr uint8_t  IDLE_SPARKLE_SATURATION = 170;   // 0 = white sparkles, 255 = vivid colors

// ---------- Round cleared ----------
constexpr uint32_t ROUND_CLEARED_MS    = 400;
constexpr uint32_t ROUND_CLEARED_COLOR = 0x00FF40;

// ---------- Game over ----------
constexpr uint32_t GAME_OVER_COLOR    = 0xFF0000;
constexpr uint8_t  GAME_OVER_FLASHES  = 4;    // all-strip red flashes (0 = off)
constexpr uint32_t GAME_OVER_FLASH_MS = 250;  // on time (off time is the same)
constexpr uint8_t  GAME_OVER_BLINKS   = 0;    // blinks of the strip you should have pressed (0 = off)
constexpr uint32_t GAME_OVER_BLINK_MS = 300;

// ---------- Victory (~6 s) ----------
constexpr uint32_t VICTORY_CHASE_MS           = 2000;  // light spinning around the 4 strips
constexpr uint32_t VICTORY_CHASE_STEP_MS      = 90;    // lower = faster spin
constexpr uint8_t  VICTORY_CHASE_TRAIL[NUM_STRIPS] = {255, 90, 25, 0};  // head, then fading tail
constexpr uint32_t VICTORY_CONFETTI_MS        = 2500;  // rainbow confetti
constexpr uint8_t  VICTORY_CONFETTI_PER_FRAME = 3;
constexpr uint8_t  VICTORY_CONFETTI_FADE      = 40;    // higher = shorter confetti trails
constexpr uint8_t  VICTORY_CONFETTI_SATURATION = 200;  // 0 = white confetti, 255 = pure colors
constexpr uint8_t  VICTORY_FLASHES            = 3;     // white flashes at the end
constexpr uint32_t VICTORY_FLASH_MS           = 200;
constexpr uint32_t VICTORY_FADE_MS            = 600;   // fade back into idle

// ---------- OTA ----------
constexpr uint32_t OTA_PROGRESS_COLOR = 0x00C8FF;
constexpr uint32_t OTA_PROGRESS_FRAME_MS = 200;  // min time between progress redraws during upload
constexpr uint8_t  OTA_PROGRESS_LEVEL = 40;   // keep low: a bright full bar draws enough power to break uploads
constexpr uint32_t OTA_ERROR_SHOW_MS  = 1000;  // red shown after a failed upload

// ---------- Flamingo pairing (hold all 4 buttons in idle) ----------
#define FLAMINGO_HOST "flamingo-esp32.local"         // mDNS name or IP address of the Flamingo
constexpr uint16_t FLAMINGO_PORT              = 5000;
constexpr uint8_t  FLAMINGO_SIMON_ID          = 5;      // stations are 1-4
constexpr uint32_t FLAMINGO_KEEPALIVE_MS      = 100;    // resend current colour this often while paired
constexpr uint8_t  FLAMINGO_REPEATS           = 3;      // win/unpair packets are sent this many times
constexpr uint32_t FLAMINGO_RESOLVE_RETRY_MS  = 10000;  // idle-only lookup retry while unresolved
constexpr uint32_t FLAMINGO_RESOLVE_TIMEOUT_MS = 300;   // max time one lookup may block
constexpr uint32_t PAIR_HOLD_MS               = 2000;   // hold all 4 buttons this long to pair
constexpr uint32_t PAIR_COLOR                 = 0xFF1493;  // pink
constexpr uint8_t  PAIR_FLASHES               = 2;      // pink flashes when paired
constexpr uint32_t PAIR_FLASH_MS              = 200;    // on time (off time is the same)

// ---------- Ethernet (WT32-ETH01 / ESP32-ETH01, LAN8720) ----------
constexpr bool ETH_ENABLED         = true;   // Ethernet first; WiFi only while the cable is down
constexpr uint8_t ETH01_PHY_ADDR   = 1;
constexpr int ETH01_PHY_POWER_PIN  = 16;
constexpr int ETH01_MDC_PIN        = 23;
constexpr int ETH01_MDIO_PIN       = 18;

// ---------- Network / system ----------
#define HOSTNAME "simon"  // device is reachable as simon.local (also update upload_port in platformio.ini)
constexpr uint32_t WIFI_RETRY_MS = 15000;
constexpr uint32_t FRAME_MS      = 16;    // ~60 fps
constexpr bool     LOG_ENABLED   = true;
constexpr uint32_t SERIAL_BAUD   = 115200;

// ---------- Sanity checks: a bad value fails the build instead of crashing the device ----------
static_assert(IDLE_BREATH_PERIOD_MS > 0 && IDLE_SHIMMER_PERIOD_MS > 0, "idle periods must be > 0");
static_assert(VICTORY_CHASE_STEP_MS > 0, "VICTORY_CHASE_STEP_MS must be > 0");
static_assert(IDLE_COMET_SPACING > IDLE_COMET_TAIL, "IDLE_COMET_SPACING must be larger than IDLE_COMET_TAIL");
static_assert(ROUND_CLEARED_MS == 0 || ROUND_CLEARED_MS >= 2, "ROUND_CLEARED_MS must be 0 or >= 2");
static_assert(ON_MS_FIRST + GAP_MS_FIRST > 0 && ON_MS_LAST + GAP_MS_LAST > 0,
              "on + gap time must be > 0 for every round");
static_assert(PULSE_FADE_PERCENT <= 100, "PULSE_FADE_PERCENT must be 0-100");
static_assert(FRAME_MS > 0, "FRAME_MS must be > 0");
static_assert(START_SEQUENCE_LEN >= 1 && START_SEQUENCE_LEN <= MAX_SEQUENCE_LEN && MAX_SEQUENCE_LEN <= 64,
              "need 1 <= START_SEQUENCE_LEN <= MAX_SEQUENCE_LEN <= 64");
static_assert(FLAMINGO_KEEPALIVE_MS > 0 && PAIR_HOLD_MS > 0 && PAIR_FLASH_MS > 0,
              "Flamingo/pairing timings must be > 0");
static_assert(FLAMINGO_SIMON_ID > 4, "FLAMINGO_SIMON_ID must not collide with stations 1-4");
