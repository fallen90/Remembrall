# Unit test for SemVer parsing / comparison (no network).

#include "system/version.h"

#include <iostream>

static int g_failures = 0;
#define EXPECT_TRUE(x) do { if (!(x)) { std::cerr << "FAIL " << #x << " @ " << __LINE__ << "\n"; ++g_failures; } } while (0)

int RunVersionTests() {
  auto a = lta::SemVer::Parse("v1.2.3");
  EXPECT_TRUE(a.major == 1 && a.minor == 2 && a.patch == 3);
  auto b = lta::SemVer::Parse("1.2.4");
  EXPECT_TRUE(b.IsNewerThan(a));
  EXPECT_TRUE(!a.IsNewerThan(b));
  EXPECT_TRUE(!a.IsNewerThan(a));
  auto c = lta::SemVer::Parse("v2.0.0-beta");
  EXPECT_TRUE(c.major == 2 && c.minor == 0 && c.patch == 0);
  EXPECT_TRUE(c.IsNewerThan(b));
  return g_failures;
}
