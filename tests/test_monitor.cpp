#include "config.h"
#include "monitor.h"
#include "simulation.h"
#include <future>
#include <gtest/gtest.h>

TEST(Monitor, PublishesSamplesAndLatestStats) {
  config = Config();
  config.numberHands = 100000;
  config.threads = 2;

  SimMonitor monitor(config.threads);
  const Stats result = runSim(&monitor);

  EXPECT_EQ(result.hands, 100000 * 2);
  for (const auto &probe : monitor.probes) {
    const int n = probe->sampleCount.load();
    EXPECT_GT(n, 0);
    EXPECT_LE(n, kMaxSamples);
    // Samples are monotonically increasing in hands played.
    for (int i = 1; i < n; ++i)
      EXPECT_GE(probe->sampleHands[i], probe->sampleHands[i - 1]);
    EXPECT_EQ(probe->readLatest().hands, 100000);
  }
}

TEST(Monitor, StopRequestEndsRunEarly) {
  config = Config();
  config.numberHands = 50000000;
  config.threads = 2;

  SimMonitor monitor(config.threads);
  auto future = std::async(std::launch::async,
                           [&monitor] { return runSim(&monitor); });
  monitor.stopRequested.store(true);
  const Stats result = future.get();

  EXPECT_LT(result.hands,
            static_cast<int64_t>(config.numberHands) * config.threads);
}

TEST(BetPercent, PercentModeScalesBetsWithBank) {
  config = Config();
  config.numberHands = 100000;
  config.threads = 1;
  config.betPercentMode = true;
  config.betPercent = 1.0f; // 1% of current bank

  const Stats result = runSim(nullptr);

  EXPECT_GT(result.hands, 0);
  EXPECT_GT(result.totalBet, 0);
  // Average bet should be near 1% of the average bank, far above the
  // raw default bet of 10 for a 100,000 starting bank.
  const double avgBet =
      static_cast<double>(result.totalBet) / static_cast<double>(result.hands);
  EXPECT_GT(avgBet, 100.0);
}

TEST(BetPercent, MinimumBetIsOne) {
  config = Config();
  config.numberHands = 1000;
  config.threads = 1;
  config.startingBank = 100;
  config.betPercentMode = true;
  config.betPercent = 0.01f; // 0.01% of 100 rounds to 0 -> clamps to 1

  const Stats result = runSim(nullptr);

  EXPECT_GT(result.hands, 0);
  EXPECT_GE(result.totalBet, result.hands); // every bet at least 1
}

TEST(BetPercent, MinimumBetFloorsFinalBet) {
  config = Config();
  config.numberHands = 1000;
  config.threads = 1;
  config.betPercentMode = true;
  config.betPercent = 0.001f; // would round to 0 on a 100,000 bank
  config.minimumBet = 50;

  const Stats result = runSim(nullptr);

  EXPECT_GT(result.hands, 0);
  // Every initial bet is at least 50 (splits/doubles only add more).
  EXPECT_GE(result.totalBet, result.hands * 50);
}

TEST(Monitor, NullMonitorLeavesSimUnchanged) {
  config = Config();
  config.numberHands = 10000;
  config.threads = 1;

  const Stats result = runSim(nullptr);
  EXPECT_EQ(result.hands, 10000);
}
