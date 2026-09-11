#include "actions.h"
#include "gtest/gtest.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <string>

namespace {

std::string writeTemp(const std::string &name, const std::string &contents) {
  const std::string path = ::testing::TempDir() + name;
  std::ofstream f(path);
  f << contents;
  f.close();
  return path;
}

} // namespace

// A chart round-trips through serialize -> parse unchanged.
TEST(StrategyIO, RoundTripsBasicStrategy) {
  const nlohmann::json j = strategyToJson(kBasicStrategy);
  StrategyTable out;
  ASSERT_TRUE(strategyFromJson(j, out));

  for (int r = 0; r < 22; ++r)
    for (int c = 0; c < 12; ++c) {
      EXPECT_EQ(out.hard[r][c], kBasicStrategy.hard[r][c]);
      EXPECT_EQ(out.soft[r][c], kBasicStrategy.soft[r][c]);
    }
  for (int r = 0; r < 12; ++r)
    for (int c = 0; c < 12; ++c)
      EXPECT_EQ(out.pair[r][c], kBasicStrategy.pair[r][c]);
}

// Loading a file that flips a known cell (hard 12 vs 4: Stand -> Hit) is
// reflected by getAction.
TEST(StrategyIO, LoadsFlippedCellFromFile) {
  nlohmann::json j = strategyToJson(kBasicStrategy);
  ASSERT_EQ(j["hard"][12][4].get<std::string>(), "S");
  j["hard"][12][4] = "H";
  const std::string path = writeTemp("bj_flip.json", j.dump());

  StrategyTable prev = gStrategy;
  gStrategy = kBasicStrategy;
  ASSERT_TRUE(loadStrategyFromJson(path, gStrategy));
  EXPECT_EQ(getAction(12, 4, false, false, 0), Action::Hit);
  gStrategy = prev;
}

// A surrender code ("R") parses to Action::Surrender.
TEST(StrategyIO, ParsesSurrenderCode) {
  nlohmann::json j = strategyToJson(kBasicStrategy);
  j["hard"][16][11] = "R";
  StrategyTable out;
  ASSERT_TRUE(strategyFromJson(j, out));
  EXPECT_EQ(out.hard[16][11], Action::Surrender);
}

// Wrong dimensions are rejected and the target is left untouched.
TEST(StrategyIO, RejectsWrongDimensions) {
  nlohmann::json j = strategyToJson(kBasicStrategy);
  j["hard"] = nlohmann::json::array(); // 0 rows instead of 22
  StrategyTable out = kBasicStrategy;
  out.hard[5][5] = Action::Surrender; // sentinel that must survive
  EXPECT_FALSE(strategyFromJson(j, out));
  EXPECT_EQ(out.hard[5][5], Action::Surrender); // unchanged
}

// An invalid cell code is rejected and the target is left untouched.
TEST(StrategyIO, RejectsBadCode) {
  nlohmann::json j = strategyToJson(kBasicStrategy);
  j["pair"][8][8] = "X";
  StrategyTable out = kBasicStrategy;
  out.pair[8][8] = Action::Hit; // sentinel
  EXPECT_FALSE(strategyFromJson(j, out));
  EXPECT_EQ(out.pair[8][8], Action::Hit); // unchanged
}

// A malformed file leaves gStrategy equal to basic strategy.
TEST(StrategyIO, MalformedFileLeavesChartUntouched) {
  const std::string path = writeTemp("bj_bad.json", "{ not valid json ]");
  StrategyTable prev = gStrategy;
  gStrategy = kBasicStrategy;
  EXPECT_FALSE(loadStrategyFromJson(path, gStrategy));
  EXPECT_EQ(getAction(12, 4, false, false, 0), Action::Stand); // basic strategy
  gStrategy = prev;
}

// A missing file returns false rather than crashing.
TEST(StrategyIO, MissingFileReturnsFalse) {
  StrategyTable out = kBasicStrategy;
  EXPECT_FALSE(loadStrategyFromJson("/no/such/strategy/file.json", out));
}
