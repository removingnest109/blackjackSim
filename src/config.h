#pragma once

struct Config {
  int numberHands = 10000000;
  int numberDecks = 6;
  int startingBank = 100000;
  int defaultBetSize = 10;
  unsigned int threads = 1;
  float penetrationBeforeShuffle = 0.75;
  bool dealerHitSoft17 = false;
  bool isInteractive = false;
  bool cardCounting = false;
  bool verbose = false;
  bool debtAllowed = false;
  bool multiThread = false;
};

extern Config config;