#include "transcript/transcript_manager.h"

#include <iostream>

static int g_failures = 0;
#define EXPECT_TRUE(x) do { if (!(x)) { std::cerr << "FAIL " << #x << " @ " << __LINE__ << "\n"; ++g_failures; } } while (0)

int RunTranscriptTests() {
  lta::TranscriptManager tm(std::chrono::minutes(45));
  const auto t0 = lta::SteadyClock::now();
  tm.UpdatePartial("hello", t0);
  tm.UpdatePartial("hello world", t0);
  auto partial = tm.ActivePartial();
  EXPECT_TRUE(partial.has_value());
  EXPECT_TRUE(partial->text == "hello world");
  EXPECT_TRUE(partial->state == lta::SegmentState::Partial);

  auto fin = tm.FinalizeCurrent(lta::SteadyClock::now());
  EXPECT_TRUE(fin.state == lta::SegmentState::Final);
  EXPECT_TRUE(fin.text == "hello world");
  EXPECT_TRUE(!tm.ActivePartial().has_value());

  auto all = tm.GetSegments();
  EXPECT_TRUE(all.size() == 1);
  EXPECT_TRUE(all[0].state == lta::SegmentState::Final);
  return g_failures;
}
