#pragma once

#include <string>

// Bet multipliers per true-count bucket when card counting.
// Buckets: <=0, <=2, <=3, <=4, <=5, >5.
inline constexpr int kBetCurveSize = 6;

struct Config {
  int numberHands = 10'000'000;
  int numberDecks = 6;
  int startingBank = 100'000;
  int defaultBetSize = 10;
  bool betPercentMode = false;
  float betPercent = 1.0f; // % of current bank, used when betPercentMode
  int minimumBet = 1;      // floor applied to the final bet in all modes
  int maximumBet = 0;      // ceiling applied to the final bet (0 = no limit)
  unsigned int threads = 1;
  float penetrationBeforeShuffle = 0.75;
  bool dealerHitSoft17 = false;
  bool surrenderAllowed = false;
  bool earlySurrender = false; // only meaningful when surrenderAllowed
  bool cardCounting = false;
  int betCurve[kBetCurveSize] = {1, 2, 3, 4, 5, 6};
  bool verbose = false;
  bool debtAllowed = false;
  bool multiThread = false;
  int playersPerTable = 1;
  std::string saveJsonPath; // when non-empty, write the run to this JSON file
  std::string strategyPath; // when non-empty, load this JSON chart into gStrategy
};

extern Config config;