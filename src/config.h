#pragma once

// Bet multipliers per true-count bucket when card counting.
// Buckets: <=0, <=2, <=3, <=4, <=5, >5.
enum { kBetCurveSize = 6 };

struct Config {
  int numberHands = 10000000;
  int numberDecks = 6;
  int startingBank = 100000;
  int defaultBetSize = 10;
  bool betPercentMode = false;
  float betPercent = 1.0f; // % of current bank, used when betPercentMode
  int minimumBet = 1;      // floor applied to the final bet in all modes
  unsigned int threads = 1;
  float penetrationBeforeShuffle = 0.75;
  bool dealerHitSoft17 = false;
  bool cardCounting = false;
  int betCurve[kBetCurveSize] = {1, 3, 6, 10, 14, 16};
  bool verbose = false;
  bool debtAllowed = false;
  bool multiThread = false;
};

extern Config config;