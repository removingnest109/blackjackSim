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

TEST(Monitor, NullMonitorLeavesSimUnchanged) {
  config = Config();
  config.numberHands = 10000;
  config.threads = 1;

  const Stats result = runSim(nullptr);
  EXPECT_EQ(result.hands, 10000);
}
