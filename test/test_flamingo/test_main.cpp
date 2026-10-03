#include <FlamingoLink.h>
#include <unity.h>

static FlamingoLinkConfig cfg() {
  FlamingoLinkConfig c;
  c.simonId = 5;
  c.keepaliveMs = 100;
  c.repeats = 3;
  return c;
}

void setUp() {}
void tearDown() {}

void test_unpaired_sends_nothing() {
  FlamingoLink link(cfg());
  uint8_t out[2];
  TEST_ASSERT_FALSE(link.paired());
  TEST_ASSERT_FALSE(link.poll(0, out));
  TEST_ASSERT_FALSE(link.poll(1000, out));
}

void test_pair_sends_pink_immediately() {
  FlamingoLink link(cfg());
  uint8_t out[2];
  link.pair();
  TEST_ASSERT_TRUE(link.paired());
  TEST_ASSERT_TRUE(link.poll(10, out));
  TEST_ASSERT_EQUAL_HEX8(5, out[0]);
  TEST_ASSERT_EQUAL_HEX8(FlamingoValue::kPink, out[1]);
}

void test_same_value_not_resent_before_keepalive() {
  FlamingoLink link(cfg());
  uint8_t out[2];
  link.pair();
  link.poll(0, out);
  TEST_ASSERT_FALSE(link.poll(16, out));
  TEST_ASSERT_FALSE(link.poll(99, out));
}

void test_keepalive_resends_after_interval() {
  FlamingoLink link(cfg());
  uint8_t out[2];
  link.pair();
  link.poll(0, out);
  TEST_ASSERT_TRUE(link.poll(100, out));
  TEST_ASSERT_EQUAL_HEX8(FlamingoValue::kPink, out[1]);
}

void test_value_change_sends_immediately() {
  FlamingoLink link(cfg());
  uint8_t out[2];
  link.pair();
  link.poll(0, out);
  link.setValue(0x04);
  TEST_ASSERT_TRUE(link.poll(5, out));
  TEST_ASSERT_EQUAL_HEX8(0x04, out[1]);
  TEST_ASSERT_FALSE(link.poll(10, out));
}

void test_set_value_ignored_when_unpaired() {
  FlamingoLink link(cfg());
  uint8_t out[2];
  link.setValue(0x01);
  TEST_ASSERT_FALSE(link.poll(0, out));
}

void test_unpair_sends_exactly_repeats_then_stops() {
  FlamingoLink link(cfg());
  uint8_t out[2];
  link.pair();
  link.poll(0, out);
  link.endWithUnpair();
  TEST_ASSERT_FALSE(link.paired());
  for (int i = 0; i < 3; i++) {
    TEST_ASSERT_TRUE(link.poll(20 + i, out));
    TEST_ASSERT_EQUAL_HEX8(5, out[0]);
    TEST_ASSERT_EQUAL_HEX8(FlamingoValue::kUnpair, out[1]);
  }
  TEST_ASSERT_FALSE(link.poll(30, out));
  TEST_ASSERT_FALSE(link.poll(500, out));
}

void test_win_sends_exactly_repeats_then_stops() {
  FlamingoLink link(cfg());
  uint8_t out[2];
  link.pair();
  link.endWithWin();
  for (int i = 0; i < 3; i++) {
    TEST_ASSERT_TRUE(link.poll(i, out));
    TEST_ASSERT_EQUAL_HEX8(FlamingoValue::kWin, out[1]);
  }
  TEST_ASSERT_FALSE(link.poll(10, out));
}

void test_end_when_not_paired_sends_nothing() {
  FlamingoLink link(cfg());
  uint8_t out[2];
  link.endWithUnpair();
  link.endWithWin();
  TEST_ASSERT_FALSE(link.poll(0, out));
}

void test_repair_after_end_starts_fresh() {
  FlamingoLink link(cfg());
  uint8_t out[2];
  link.pair();
  link.endWithUnpair();
  link.pair();
  TEST_ASSERT_TRUE(link.poll(0, out));
  TEST_ASSERT_EQUAL_HEX8(FlamingoValue::kPink, out[1]);
}

void test_for_color_maps_to_station_bits() {
  TEST_ASSERT_EQUAL_HEX8(0x01, FlamingoValue::forColor(0));
  TEST_ASSERT_EQUAL_HEX8(0x02, FlamingoValue::forColor(1));
  TEST_ASSERT_EQUAL_HEX8(0x04, FlamingoValue::forColor(2));
  TEST_ASSERT_EQUAL_HEX8(0x08, FlamingoValue::forColor(3));
  TEST_ASSERT_EQUAL_HEX8(FlamingoValue::kDark, FlamingoValue::forColor(-1));
  TEST_ASSERT_EQUAL_HEX8(FlamingoValue::kDark, FlamingoValue::forColor(4));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_unpaired_sends_nothing);
  RUN_TEST(test_pair_sends_pink_immediately);
  RUN_TEST(test_same_value_not_resent_before_keepalive);
  RUN_TEST(test_keepalive_resends_after_interval);
  RUN_TEST(test_value_change_sends_immediately);
  RUN_TEST(test_set_value_ignored_when_unpaired);
  RUN_TEST(test_unpair_sends_exactly_repeats_then_stops);
  RUN_TEST(test_win_sends_exactly_repeats_then_stops);
  RUN_TEST(test_end_when_not_paired_sends_nothing);
  RUN_TEST(test_repair_after_end_starts_fresh);
  RUN_TEST(test_for_color_maps_to_station_bits);
  return UNITY_END();
}
