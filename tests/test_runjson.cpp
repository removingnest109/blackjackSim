#include "config.h"
#include "monitor.h"
#include "runjson.h"
#include "simulation.h"
#include "stats.h"

#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace {

// Configure a small, deterministic-shaped run so the export finishes quickly
// while still producing a full timeline.
void configureSmallRun() {
  config.numberHands = 20000;
  config.numberDecks = 6;
  config.startingBank = 100000;
  config.defaultBetSize = 10;
  config.betPercentMode = false;
  config.betPercent = 1.0f;
  config.minimumBet = 1;
  config.maximumBet = 0;
  config.threads = 2;
  config.penetrationBeforeShuffle = 0.75f;
  config.dealerHitSoft17 = false;
  config.cardCounting = false;
  config.debtAllowed = false;
  config.multiThread = false;
  config.playersPerTable = 1;
}

} // namespace

TEST(RunJson, WritesGuiCompatibleRun) {
  configureSmallRun();

  SimMonitor monitor(config.threads, config.playersPerTable,
                     config.startingBank);
  const Stats result = runSim(&monitor);

  const std::string path = "test_runjson_output.json";
  ASSERT_TRUE(saveRunJson(path, monitor, result, 1.25));

  std::ifstream f(path.c_str());
  ASSERT_TRUE(f.is_open());
  nlohmann::json root;
  ASSERT_NO_THROW(f >> root);

  EXPECT_EQ(root.value("version", 0), 1);
  ASSERT_TRUE(root.contains("runs"));
  ASSERT_EQ(root["runs"].size(), 1u);

  const nlohmann::json &run = root["runs"][0];
  EXPECT_EQ(run.value("stopped", true), false);
  EXPECT_EQ(run.value("elapsed", 0.0), 1.25);

  // Stats round-trip through the shared encoder.
  ASSERT_TRUE(run.contains("stats"));
  EXPECT_EQ(run["stats"].value("hands", int64_t(-1)), result.hands);
  EXPECT_EQ(run["stats"].value("bank", int64_t(-1)), result.bank);

  // One timeline series per table * player, each with aligned, non-empty x/y.
  ASSERT_TRUE(run.contains("threads"));
  EXPECT_EQ(run["threads"].size(),
            static_cast<size_t>(config.threads) * config.playersPerTable);
  for (const auto &series : run["threads"]) {
    ASSERT_TRUE(series.contains("x"));
    ASSERT_TRUE(series.contains("y"));
    EXPECT_FALSE(series["x"].empty());
    EXPECT_EQ(series["x"].size(), series["y"].size());
  }

  // The cross-thread average is present for overlay in the GUI.
  ASSERT_TRUE(run.contains("avg"));
  EXPECT_FALSE(run["avg"]["y"].empty());

  std::remove(path.c_str());
}
