#include "actions.h"
#include "gtest/gtest.h"

// The runtime chart must default to canonical basic strategy so behaviour is
// unchanged until someone overrides gStrategy.
TEST(StrategyTable, DefaultMatchesBasicStrategyKnownCells) {
  gStrategy = kBasicStrategy;

  // Hard 16 vs dealer 10 -> Hit.
  EXPECT_EQ(getAction(16, 10, /*isSoft=*/false, /*isPair=*/false, 0),
            Action::Hit);
  // Hard 13 vs dealer 4 -> Stand.
  EXPECT_EQ(getAction(13, 4, false, false, 0), Action::Stand);
  // Hard 11 vs dealer 6 -> Double.
  EXPECT_EQ(getAction(11, 6, false, false, 0), Action::Double);
  // Soft 18 (A7) vs dealer 9 -> Hit.
  EXPECT_EQ(getAction(18, 9, /*isSoft=*/true, false, 0), Action::Hit);
  // Soft 18 (A7) vs dealer 2 -> Stand.
  EXPECT_EQ(getAction(18, 2, true, false, 0), Action::Stand);
  // Pair 8,8 vs dealer 10 -> Split (pairRank 8).
  EXPECT_EQ(getAction(16, 10, false, /*isPair=*/true, 8), Action::Split);
  // Pair 10,10 vs dealer 6 -> Stand (pairRank 10).
  EXPECT_EQ(getAction(20, 6, false, true, 10), Action::Stand);
}

// getAction reads the live gStrategy, so mutating a cell changes the result.
TEST(StrategyTable, RespectsRuntimeOverride) {
  gStrategy = kBasicStrategy;
  ASSERT_EQ(getAction(12, 4, false, false, 0), Action::Stand);

  gStrategy.hard[12][4] = Action::Hit;
  EXPECT_EQ(getAction(12, 4, false, false, 0), Action::Hit);

  gStrategy = kBasicStrategy; // restore for other tests
}
