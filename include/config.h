#pragma once
#include <stdint.h>

// =====================================================================
//  Simon — every tunable setting lives in this file.
//  Edit a value, then re-flash over WiFi:   pio run -e ota -t upload
//  Strip/button order everywhere: 0 = red, 1 = green, 2 = blue, 3 = yellow
// =====================================================================

// ---------- Pins (ESP32 GPIO numbers) ----------
#define BUTTON_PIN_RED     13
#define BUTTON_PIN_GREEN   12
#define BUTTON_PIN_BLUE    14
#define BUTTON_PIN_YELLOW  27

#define LED_PIN_RED        2
#define LED_PIN_GREEN      4
#define LED_PIN_BLUE       5
#define LED_PIN_YELLOW     18

// ---------- LED strips ----------
#define LED_TYPE           WS2812B
#define COLOR_ORDER        GRB          // try RGB if red and green look swapped
constexpr uint8_t  NUM_STRIPS      = 4;
constexpr uint8_t  LEDS_PER_STRIP  = 8;
constexpr uint8_t  MAX_BRIGHTNESS  = 160;   // global brightness cap, 0-255
constexpr uint32_t POWER_LIMIT_MA  = 1500;  // FastLED dims everything to stay under this (5 V)
// Strip colors as 0xRRGGBB. Pure yellow (0xFFFF00) looks greenish on WS2812B, so it is warmed up.
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
constexpr uint8_t  IDLE_MAX_BRIGHTNESS    = 90;     // 0-255, applied on top of MAX_BRIGHTNESS
constexpr float    IDLE_MIN_LEVEL         = 0.15f;  // dimmest point of the breath (0-1)
constexpr uint32_t IDLE_BREATH_PERIOD_MS  = 4000;   // one breath; strips are offset so it rotates
constexpr uint32_t IDLE_SHIMMER_PERIOD_MS = 1800;   // wave travelling along each strip
constexpr float    IDLE_SHIMMER_SPACING   = 0.8f;   // wave phase between neighbouring LEDs (radians)
constexpr float    IDLE_SHIMMER_DEPTH     = 0.35f;  // 0 = no shimmer, 1 = full
constexpr uint8_t  IDLE_SPARKLE_CHANCE    = 6;      // per strip per frame, out of 255
constexpr uint8_t  IDLE_SPARKLE_LEVEL     = 200;    // sparkle peak brightness
constexpr uint8_t  IDLE_SPARKLE_DECAY     = 12;     // sparkle fade per frame

// ---------- Round cleared ----------
constexpr uint32_t ROUND_CLEARED_MS    = 400;
constexpr uint32_t ROUND_CLEARED_COLOR = 0x00FF40;

// ---------- Game over ----------
constexpr uint32_t GAME_OVER_COLOR    = 0xFF0000;
constexpr uint8_t  GAME_OVER_FLASHES  = 2;    // all-strip red flashes
constexpr uint32_t GAME_OVER_FLASH_MS = 250;  // on time (off time is the same)
constexpr uint8_t  GAME_OVER_BLINKS   = 3;    // blinks of the strip you should have pressed
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
constexpr uint32_t OTA_ERROR_SHOW_MS  = 1000;  // red shown after a failed upload

// ---------- Network / system ----------
#define HOSTNAME "simon"  // device is reachable as simon.local (also update upload_port in platformio.ini)
constexpr uint32_t WIFI_RETRY_MS = 15000;
constexpr uint32_t FRAME_MS      = 16;    // ~60 fps
constexpr bool     LOG_ENABLED   = true;
constexpr uint32_t SERIAL_BAUD   = 115200;

// ---------- Sanity checks: a bad value fails the build instead of crashing the device ----------
static_assert(IDLE_BREATH_PERIOD_MS > 0 && IDLE_SHIMMER_PERIOD_MS > 0, "idle periods must be > 0");
static_assert(VICTORY_CHASE_STEP_MS > 0, "VICTORY_CHASE_STEP_MS must be > 0");
static_assert(ROUND_CLEARED_MS == 0 || ROUND_CLEARED_MS >= 2, "ROUND_CLEARED_MS must be 0 or >= 2");
static_assert(ON_MS_FIRST + GAP_MS_FIRST > 0 && ON_MS_LAST + GAP_MS_LAST > 0,
              "on + gap time must be > 0 for every round");
static_assert(PULSE_FADE_PERCENT <= 100, "PULSE_FADE_PERCENT must be 0-100");
static_assert(FRAME_MS > 0, "FRAME_MS must be > 0");
static_assert(START_SEQUENCE_LEN >= 1 && START_SEQUENCE_LEN <= MAX_SEQUENCE_LEN && MAX_SEQUENCE_LEN <= 64,
              "need 1 <= START_SEQUENCE_LEN <= MAX_SEQUENCE_LEN <= 64");
