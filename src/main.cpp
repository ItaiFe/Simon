#include <Arduino.h>
#include <SimonGame.h>
#include "Animations.h"
#include "Buttons.h"
#include "Leds.h"
#include "Log.h"
#include "Net.h"
#include "config.h"

enum class State : uint8_t { Boot, Idle, GetReady, Showing, Input, RoundCleared, Victory, GameOver };

static const char* const COLOR_NAMES[NUM_STRIPS] = {"red", "green", "blue", "yellow"};

static const char* stateName(State s) {
  switch (s) {
    case State::Boot: return "Boot";
    case State::Idle: return "Idle";
    case State::GetReady: return "GetReady";
    case State::Showing: return "Showing";
    case State::Input: return "Input";
    case State::RoundCleared: return "RoundCleared";
    case State::Victory: return "Victory";
    case State::GameOver: return "GameOver";
  }
  return "?";
}

static SimonConfig makeGameConfig() {
  SimonConfig c;
  c.startLen = START_SEQUENCE_LEN;
  c.maxLen = MAX_SEQUENCE_LEN;
  c.onMsFirst = ON_MS_FIRST;
  c.onMsLast = ON_MS_LAST;
  c.gapMsFirst = GAP_MS_FIRST;
  c.gapMsLast = GAP_MS_LAST;
  c.timeoutMsFirst = TIMEOUT_MS_FIRST;
  c.timeoutMsLast = TIMEOUT_MS_LAST;
  return c;
}

static SimonGame game(makeGameConfig());
static Buttons buttons;
static State state = State::Boot;
static uint32_t stateStart = 0;
static uint32_t lastFrame = 0;
static uint32_t lastPressAt = 0;
static int8_t pulseStrip = -1;  // strip lit by the player's latest press, -1 = none
static uint32_t pulseStart = 0;

static void enter(State next, uint32_t now) {
  state = next;
  stateStart = now;
  // Presses made during playback/animations must not count as moves.
  if (next == State::Idle || next == State::Input) buttons.clear();
  if (next == State::Input) {
    lastPressAt = now;
    pulseStrip = -1;
  }
  LOG("-> %s", stateName(next));
}

static void logRound() {
  const SimonTiming tm = game.timing();
  String seq;
  for (uint8_t i = 0; i < game.sequenceLength(); i++) {
    seq += COLOR_NAMES[game.colorAt(i)];
    seq += ' ';
  }
  LOG("Round %u/%u: %s(on %u ms, gap %u ms, timeout %u ms)", game.round(), game.totalRounds(),
      seq.c_str(), tm.onMs, tm.gapMs, tm.timeoutMs);
}

static void onOtaProgress(float fraction) {
  // Called for every network packet; redrawing 400 LEDs each time starves WiFi and breaks uploads.
  static uint32_t lastDraw = 0;
  const uint32_t now = millis();
  if (now - lastDraw < OTA_PROGRESS_FRAME_MS && fraction > 0.0f && fraction < 1.0f) return;
  lastDraw = now;
  Animations::otaProgress(fraction);
  Leds::show();
}

static void onOtaEnd(bool ok) {
  Animations::otaResult(ok);
  Leds::show();
  if (!ok) {
    delay(OTA_ERROR_SHOW_MS);  // show the red result before returning to idle
    enter(State::Idle, millis());
  }
}

static void handlePress(int8_t color, uint32_t now) {
  const uint8_t expected = game.expectedColor();
  const PressResult r = game.press(color);
  pulseStrip = color;
  pulseStart = now;
  lastPressAt = now;
  switch (r) {
    case PressResult::Correct:
      LOG("Press %s: correct", COLOR_NAMES[color]);
      break;
    case PressResult::RoundComplete:
      LOG("Press %s: round %u complete", COLOR_NAMES[color], game.round());
      enter(State::RoundCleared, now);
      break;
    case PressResult::Won:
      LOG("Press %s: WON!", COLOR_NAMES[color]);
      enter(State::Victory, now);
      break;
    case PressResult::Wrong:
      LOG("Press %s: wrong, expected %s", COLOR_NAMES[color], COLOR_NAMES[expected]);
      enter(State::GameOver, now);
      break;
  }
}

// Advances the state machine and draws one frame.
static void step(uint32_t now) {
  const uint32_t t = now - stateStart;
  const int8_t press = buttons.pressed();

  switch (state) {
    case State::Boot:
      if (Animations::boot(t, now)) enter(State::Idle, now);
      break;

    case State::Idle:
      Animations::idle(now);
      if (press >= 0) {
        LOG("Start pressed (%s)", COLOR_NAMES[press]);
        game.start(esp_random());
        enter(State::GetReady, now);
      }
      break;

    case State::GetReady:
      if (Animations::getReady(t)) {
        logRound();
        enter(State::Showing, now);
      }
      break;

    case State::Showing: {
      if (t < SHOW_LEAD_IN_MS) {
        Leds::clear();
        break;
      }
      const SimonTiming tm = game.timing();
      const uint32_t stepMs = tm.onMs + tm.gapMs;
      const uint32_t elapsed = t - SHOW_LEAD_IN_MS;
      const uint32_t idx = elapsed / stepMs;
      if (idx >= game.sequenceLength()) {
        Leds::clear();
        enter(State::Input, now);
        break;
      }
      const uint32_t within = elapsed % stepMs;
      if (within < tm.onMs) Animations::pulse(game.colorAt(idx), within, tm.onMs);
      else Leds::clear();
      break;
    }

    case State::Input:
      if (pulseStrip >= 0 && Animations::pulse(pulseStrip, now - pulseStart, PRESS_PULSE_MS))
        pulseStrip = -1;
      if (pulseStrip < 0) Leds::clear();
      if (press >= 0) {
        handlePress(press, now);
      } else if (now - lastPressAt > game.timing().timeoutMs) {
        LOG("Timeout, expected %s", COLOR_NAMES[game.expectedColor()]);
        enter(State::GameOver, now);
      }
      break;

    case State::RoundCleared:
      // Finish the last press's pulse, then the green "cleared" pulse.
      if (t < PRESS_PULSE_MS) {
        Animations::pulse(pulseStrip, t, PRESS_PULSE_MS);
      } else if (Animations::roundCleared(t - PRESS_PULSE_MS)) {
        game.nextRound();
        logRound();
        enter(State::Showing, now);
      }
      break;

    case State::Victory:
      if (t < PRESS_PULSE_MS) {
        Animations::pulse(pulseStrip, t, PRESS_PULSE_MS);
      } else if (Animations::victory(t - PRESS_PULSE_MS, now)) {
        enter(State::Idle, now);
      }
      break;

    case State::GameOver:
      if (Animations::gameOver(t, game.expectedColor())) enter(State::Idle, now);
      break;
  }

  Leds::show();
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  LOG("Simon booting");
  Leds::begin();
  buttons.begin();
  Net::begin(onOtaProgress, onOtaEnd);
  enter(State::Boot, millis());
}

void loop() {
  const uint32_t now = millis();
  Net::update(now);
  buttons.update(now);
  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    step(now);
  }
}
