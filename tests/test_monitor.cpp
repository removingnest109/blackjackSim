#include "config.h"
#include "monitor.h"
#include "series.h"
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

// Regression: the worker pool used to ignore stopRequested when claiming work,
// so a stop only took effect once every queued table had been dequeued and run
// through a full probe interval. With many tables that took seconds, and the
// Stop button looked like it had done nothing. Stopping before the run starts
// makes that deterministic: no table should be claimed at all.
TEST(Monitor, StopBeforeStartClaimsNoTables) {
  config = Config();
  config.numberHands = 10000000;
  config.threads = 256;

  SimMonitor monitor(config.threads);
  monitor.stopRequested.store(true);
  const Stats result = runSim(&monitor);

  EXPECT_EQ(result.hands, 0);
  // Regression: unclaimed tables used to be dropped from the merge entirely,
  // so their players' starting banks vanished from the total and profit
  // (bank - startingBank * tables) showed a phantom loss. Players at a table
  // that never played still hold their full starting bank.
  EXPECT_EQ(result.bank,
            static_cast<int64_t>(config.threads) * config.startingBank);
}

// Regression: the GUI live view sums probe->readLatest() across all tables and
// subtracts every table's starting bank. Probes for tables still waiting in
// the work queue (tables > worker threads) had a zero-bank snapshot, so live
// profit read as a huge loss until every table had been claimed.
TEST(Monitor, UnstartedProbeReportsStartingBank) {
  const int players = 3;
  const int64_t startingBank = 500;
  SimMonitor monitor(4, players, startingBank);

  for (const auto &probe : monitor.probes) {
    EXPECT_EQ(probe->sampleCount.load(), 0);
    EXPECT_EQ(probe->readLatest().bank, startingBank * players);
    EXPECT_EQ(probe->readLatest().hands, 0);
  }
}

// A table claimed while a run is live must notice the stop within the polling
// cadence, not at the next probe publish (which scales with numberHands).
TEST(Monitor, StopIsHonouredWithinPollingCadence) {
  config = Config();
  config.numberHands = 100000000; // probe interval ~24k hands
  config.threads = 4;

  SimMonitor monitor(config.threads);
  auto future = std::async(std::launch::async,
                           [&monitor] { return runSim(&monitor); });
  monitor.stopRequested.store(true);
  const Stats result = future.get();

  // Each table polls every 1024 hands; allow generous slack for tables already
  // in flight while still failing if a full probe interval elapsed per table.
  EXPECT_LT(result.hands,
            static_cast<int64_t>(config.threads) * 1024 * 16);
}

// Regression: the cross-table average line took its x values from series 0
// alone. A bankrupted table stops playing hands, so its sampled x freezes —
// and when that table was series 0, the average line stopped advancing at the
// bankruptcy point even though other tables kept playing. The average's x must
// keep advancing while any series does.
TEST(AverageSeries, XAdvancesWhileAnySeriesStillPlays) {
  // Series 0 goes bust after sample 1: its x freezes at 20.
  const std::vector<std::vector<double>> xs = {{10, 20, 20, 20},
                                               {10, 20, 30, 40}};
  const std::vector<std::vector<double>> ys = {{0, 0, 0, 0},
                                               {100, 90, 80, 70}};
  std::vector<double> avgX, avgY;
  buildAverageSeries(xs, ys, avgX, avgY);

  ASSERT_EQ(avgX.size(), 4u);
  EXPECT_EQ(avgX[2], 30.0);
  EXPECT_EQ(avgX[3], 40.0);
  EXPECT_EQ(avgY[3], 35.0);
}

TEST(AverageSeries, TruncatesToShortestSeriesAndHandlesEmpty) {
  std::vector<double> avgX, avgY;
  buildAverageSeries({}, {}, avgX, avgY);
  EXPECT_TRUE(avgX.empty());

  const std::vector<std::vector<double>> xs = {{10, 20, 30}, {10, 20}};
  const std::vector<std::vector<double>> ys = {{1, 2, 3}, {3, 4}};
  buildAverageSeries(xs, ys, avgX, avgY);
  ASSERT_EQ(avgX.size(), 2u);
  EXPECT_EQ(avgY[1], 3.0);
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

// Bankrupt means the bank can no longer cover the table minimum. A player
// whose bank covers the minimum but not their intended bet goes all-in
// instead; the old behavior froze them in limbo (not playing, yet never
// counted bankrupt because bank >= minimum bet).
TEST(ShortStack, AllInWhenBankBelowIntendedBet) {
  config = Config();
  config.numberHands = 100;
  config.threads = 1;
  config.startingBank = 5; // covers minimumBet (1) but not defaultBetSize (10)

  const Stats result = runSim(nullptr);

  EXPECT_GT(result.hands, 0);    // plays all-in instead of freezing
  EXPECT_GE(result.totalBet, 5); // first wager is the whole bank
}

TEST(ShortStack, StopsOnlyBelowMinimumBet) {
  config = Config();
  config.numberHands = 100;
  config.threads = 1;
  config.minimumBet = 10;
  config.startingBank = 7; // below the table minimum: bankrupt, never plays

  const Stats result = runSim(nullptr);

  EXPECT_EQ(result.hands, 0);
  EXPECT_EQ(result.bank, 7);
}

// Doubles and splits put a second hand.bet on the table; without debt they
// must only be offered when the bank still covers that amount.
TEST(ShortStack, UncoveredSplitPlaysAsHardTotal) {
  config = Config();
  Stats stats;
  stats.bank = 0; // initial bet already on the table, nothing left

  std::vector<int> deck = {10, 10, 10, 10};
  Hand dealer;
  dealer.cards[0] = 6;
  dealer.cardCount = 1;

  Hand hands[4];
  int handCount = 1;
  hands[0].cards[0] = 8; // pair of 8s: always split when affordable
  hands[0].cards[1] = 8;
  hands[0].cardCount = 2;
  hands[0].value = 16;
  hands[0].bet = 10;

  simulatePlayerHands(deck, hands, handCount, dealer, stats);

  EXPECT_EQ(stats.splits, 0); // played as hard 16 vs 6: stand
  EXPECT_EQ(handCount, 1);
  EXPECT_EQ(stats.bank, 0); // bank untouched, never negative
}

TEST(ShortStack, UncoveredDoubleHitsInstead) {
  config = Config();
  Stats stats;
  stats.bank = 0;

  std::vector<int> deck = {9}; // hit card: 11 + 9 = 20, then stand
  Hand dealer;
  dealer.cards[0] = 6;
  dealer.cardCount = 1;

  Hand hands[4];
  int handCount = 1;
  hands[0].cards[0] = 6; // hard 11 vs 6: double when affordable
  hands[0].cards[1] = 5;
  hands[0].cardCount = 2;
  hands[0].value = 11;
  hands[0].bet = 10;

  simulatePlayerHands(deck, hands, handCount, dealer, stats);

  EXPECT_EQ(stats.doubles, 0);
  EXPECT_EQ(hands[0].value, 20);
  EXPECT_EQ(stats.bank, 0);
}

TEST(ShortStack, CoveredDoubleStillDoubles) {
  config = Config();
  Stats stats;
  stats.bank = 10; // exactly covers the second bet

  std::vector<int> deck = {9};
  Hand dealer;
  dealer.cards[0] = 6;
  dealer.cardCount = 1;

  Hand hands[4];
  int handCount = 1;
  hands[0].cards[0] = 6;
  hands[0].cards[1] = 5;
  hands[0].cardCount = 2;
  hands[0].value = 11;
  hands[0].bet = 10;

  simulatePlayerHands(deck, hands, handCount, dealer, stats);

  EXPECT_EQ(stats.doubles, 1);
  EXPECT_EQ(stats.bank, 0);
}

TEST(ShortStack, DebtAllowedKeepsUncoveredDoubles) {
  config = Config();
  config.debtAllowed = true;
  Stats stats;
  stats.bank = 0;

  std::vector<int> deck = {9};
  Hand dealer;
  dealer.cards[0] = 6;
  dealer.cardCount = 1;

  Hand hands[4];
  int handCount = 1;
  hands[0].cards[0] = 6;
  hands[0].cards[1] = 5;
  hands[0].cardCount = 2;
  hands[0].value = 11;
  hands[0].bet = 10;

  simulatePlayerHands(deck, hands, handCount, dealer, stats);

  EXPECT_EQ(stats.doubles, 1);
  EXPECT_EQ(stats.bank, -10);
}

TEST(Monitor, NullMonitorLeavesSimUnchanged) {
  config = Config();
  config.numberHands = 10000;
  config.threads = 1;

  const Stats result = runSim(nullptr);
  EXPECT_EQ(result.hands, 10000);
}
