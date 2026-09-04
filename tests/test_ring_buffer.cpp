#include "audio/ring_buffer.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

static int g_failures = 0;

#define EXPECT_TRUE(x) do { if (!(x)) { std::cerr << "FAIL " << #x << " @ " << __LINE__ << "\n"; ++g_failures; } } while (0)

void TestRingBufferBasic() {
  lta::RingBuffer rb(64);
  const char* msg = "hello-world-test";
  const size_t n = std::strlen(msg);
  EXPECT_TRUE(rb.Write(reinterpret_cast<const uint8_t*>(msg), n) == n);
  EXPECT_TRUE(rb.Size() == n);
  std::vector<uint8_t> out(n);
  EXPECT_TRUE(rb.Read(out.data(), n) == n);
  EXPECT_TRUE(std::memcmp(out.data(), msg, n) == 0);
  EXPECT_TRUE(rb.Size() == 0);
}

void TestRingBufferDropWhenFull() {
  lta::RingBuffer rb(16);
  std::vector<uint8_t> data(32, 0xAB);
  const size_t written = rb.Write(data.data(), data.size());
  EXPECT_TRUE(written == 16);
  EXPECT_TRUE(rb.OccupancyPercent() == 100.0);
}

int RunRingBufferTests() {
  TestRingBufferBasic();
  TestRingBufferDropWhenFull();
  return g_failures;
}
