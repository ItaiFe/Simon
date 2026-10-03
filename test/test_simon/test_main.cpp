#include <SimonGame.h>
#include <unity.h>

#define ASSERT_RESULT(expected, actual) \
  TEST_ASSERT_EQUAL_INT((int)(expected), (int)(actual))

static SimonConfig defaultCfg() {
  SimonConfig c;
  c.startLen = 3;
  c.maxLen = 10;
  c.onMsFirst = 600;
  c.onMsLast = 250;
  c.gapMsFirst = 250;
  c.gapMsLast = 100;
  c.timeoutMsFirst = 5000;
  c.timeoutMsLast = 3000;
  return c;
}

// Presses the whole current round correctly; returns the last press result.
static PressResult playRound(SimonGame& g) {
  PressResult r = PressResult::Wrong;
  for (uint8_t i = 0; i < g.sequenceLength(); i++) r = g.press(g.colorAt(i));
  return r;
}

static uint8_t otherColor(uint8_t c) { return (c + 1) % SimonGame::kNumColors; }

void setUp() {}
void tearDown() {}

void test_round_one_has_start_length_and_eight_rounds() {
  SimonGame g(defaultCfg());
  g.start(42);
  TEST_ASSERT_EQUAL_UINT8(1, g.round());
  TEST_ASSERT_EQUAL_UINT8(3, g.sequenceLength());
  TEST_ASSERT_EQUAL_UINT8(8, g.totalRounds());
}

void test_sequence_colors_are_valid() {
  SimonGame g(defaultCfg());
  g.start(123);
  for (uint8_t i = 0; i < 10; i++) TEST_ASSERT_LESS_THAN_UINT8(4, g.colorAt(i));
}

void test_same_seed_same_sequence_different_seed_differs() {
  SimonGame a(defaultCfg()), b(defaultCfg()), c(defaultCfg());
  a.start(7);
  b.start(7);
  c.start(8);
  bool differs = false;
  for (uint8_t i = 0; i < 10; i++) {
    TEST_ASSERT_EQUAL_UINT8(a.colorAt(i), b.colorAt(i));
    if (a.colorAt(i) != c.colorAt(i)) differs = true;
  }
  TEST_ASSERT_TRUE(differs);
}

void test_seed_zero_is_not_degenerate() {
  SimonGame g(defaultCfg());
  g.start(0);
  bool varied = false;
  for (uint8_t i = 1; i < 10; i++)
    if (g.colorAt(i) != g.colorAt(0)) varied = true;
  TEST_ASSERT_TRUE(varied);
}

void test_correct_presses_then_round_complete() {
  SimonGame g(defaultCfg());
  g.start(1);
  ASSERT_RESULT(PressResult::Correct, g.press(g.colorAt(0)));
  ASSERT_RESULT(PressResult::Correct, g.press(g.colorAt(1)));
  ASSERT_RESULT(PressResult::RoundComplete, g.press(g.colorAt(2)));
}

void test_next_round_appends_one_color_keeping_prefix() {
  SimonGame g(defaultCfg());
  g.start(5);
  uint8_t before[3] = {g.colorAt(0), g.colorAt(1), g.colorAt(2)};
  playRound(g);
  g.nextRound();
  TEST_ASSERT_EQUAL_UINT8(2, g.round());
  TEST_ASSERT_EQUAL_UINT8(4, g.sequenceLength());
  for (uint8_t i = 0; i < 3; i++) TEST_ASSERT_EQUAL_UINT8(before[i], g.colorAt(i));
}

void test_full_game_ends_with_won_at_ten_colors() {
  SimonGame g(defaultCfg());
  g.start(99);
  for (uint8_t r = 1; r < 8; r++) {
    ASSERT_RESULT(PressResult::RoundComplete, playRound(g));
    g.nextRound();
  }
  TEST_ASSERT_EQUAL_UINT8(8, g.round());
  TEST_ASSERT_EQUAL_UINT8(10, g.sequenceLength());
  ASSERT_RESULT(PressResult::Won, playRound(g));
}

void test_wrong_press_reports_expected_color() {
  SimonGame g(defaultCfg());
  g.start(3);
  g.press(g.colorAt(0));
  uint8_t expected = g.colorAt(1);
  TEST_ASSERT_EQUAL_UINT8(expected, g.expectedColor());
  ASSERT_RESULT(PressResult::Wrong, g.press(otherColor(expected)));
  TEST_ASSERT_EQUAL_UINT8(expected, g.expectedColor());
}

void test_presses_after_game_over_are_wrong() {
  SimonGame g(defaultCfg());
  g.start(3);
  g.press(otherColor(g.colorAt(0)));
  ASSERT_RESULT(PressResult::Wrong, g.press(g.colorAt(0)));
}

void test_presses_after_win_are_wrong() {
  SimonConfig cfg = defaultCfg();
  cfg.maxLen = 3;  // single round
  SimonGame g(cfg);
  g.start(11);
  ASSERT_RESULT(PressResult::Won, playRound(g));
  ASSERT_RESULT(PressResult::Wrong, g.press(g.colorAt(0)));
}

void test_extra_press_before_next_round_is_wrong() {
  SimonGame g(defaultCfg());
  g.start(4);
  playRound(g);
  ASSERT_RESULT(PressResult::Wrong, g.press(g.colorAt(3)));
}

void test_out_of_range_color_is_wrong() {
  SimonGame g(defaultCfg());
  g.start(6);
  ASSERT_RESULT(PressResult::Wrong, g.press(7));
}

void test_restart_resets_round_and_input() {
  SimonGame g(defaultCfg());
  g.start(9);
  playRound(g);
  g.nextRound();
  g.press(g.colorAt(0));
  g.start(10);
  TEST_ASSERT_EQUAL_UINT8(1, g.round());
  TEST_ASSERT_EQUAL_UINT8(3, g.sequenceLength());
  ASSERT_RESULT(PressResult::Correct, g.press(g.colorAt(0)));
}

void test_timing_first_middle_last_round() {
  SimonGame g(defaultCfg());
  g.start(1);
  SimonTiming t = g.timing();
  TEST_ASSERT_EQUAL_UINT16(600, t.onMs);
  TEST_ASSERT_EQUAL_UINT16(250, t.gapMs);
  TEST_ASSERT_EQUAL_UINT16(5000, t.timeoutMs);
  for (uint8_t r = 1; r < 4; r++) { playRound(g); g.nextRound(); }  // round 4
  t = g.timing();
  TEST_ASSERT_EQUAL_UINT16(450, t.onMs);       // 600 + (-350*3/7)
  TEST_ASSERT_EQUAL_UINT16(186, t.gapMs);      // 250 + (-150*3/7) truncated
  TEST_ASSERT_EQUAL_UINT16(4143, t.timeoutMs); // 5000 + (-2000*3/7) truncated
  for (uint8_t r = 4; r < 8; r++) { playRound(g); g.nextRound(); }  // round 8
  t = g.timing();
  TEST_ASSERT_EQUAL_UINT16(250, t.onMs);
  TEST_ASSERT_EQUAL_UINT16(100, t.gapMs);
  TEST_ASSERT_EQUAL_UINT16(3000, t.timeoutMs);
}

void test_config_values_are_clamped() {
  SimonConfig cfg = defaultCfg();
  cfg.startLen = 0;
  cfg.maxLen = 200;
  SimonGame g(cfg);
  g.start(2);
  TEST_ASSERT_EQUAL_UINT8(1, g.sequenceLength());
  TEST_ASSERT_EQUAL_UINT8(SimonGame::kMaxCapacity, g.totalRounds());

  cfg.startLen = 12;
  cfg.maxLen = 10;
  SimonGame h(cfg);
  h.start(2);
  TEST_ASSERT_EQUAL_UINT8(1, h.totalRounds());
  TEST_ASSERT_EQUAL_UINT8(10, h.sequenceLength());
}

void test_single_round_game_uses_first_round_timing() {
  SimonConfig cfg = defaultCfg();
  cfg.maxLen = 3;
  SimonGame g(cfg);
  g.start(1);
  TEST_ASSERT_EQUAL_UINT16(600, g.timing().onMs);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_round_one_has_start_length_and_eight_rounds);
  RUN_TEST(test_sequence_colors_are_valid);
  RUN_TEST(test_same_seed_same_sequence_different_seed_differs);
  RUN_TEST(test_seed_zero_is_not_degenerate);
  RUN_TEST(test_correct_presses_then_round_complete);
  RUN_TEST(test_next_round_appends_one_color_keeping_prefix);
  RUN_TEST(test_full_game_ends_with_won_at_ten_colors);
  RUN_TEST(test_wrong_press_reports_expected_color);
  RUN_TEST(test_presses_after_game_over_are_wrong);
  RUN_TEST(test_presses_after_win_are_wrong);
  RUN_TEST(test_extra_press_before_next_round_is_wrong);
  RUN_TEST(test_out_of_range_color_is_wrong);
  RUN_TEST(test_restart_resets_round_and_input);
  RUN_TEST(test_timing_first_middle_last_round);
  RUN_TEST(test_config_values_are_clamped);
  RUN_TEST(test_single_round_game_uses_first_round_timing);
  return UNITY_END();
}
