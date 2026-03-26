#pragma once

#include "blackjack.h"
#include "stats.h"
#include <cstdint>
#include <random>

void playPlayerHands(std::vector<int> &deck, Hand hands[], int &handCount,
                     const Hand &dealer, Stats &stats);
void turnFull(std::vector<int> &deck, Hand &dealer, std::mt19937 &rng,
              const int64_t &bet, Stats &stats);
void playHand(std::vector<int> &deck, Hand &dealer, std::mt19937 &rng,
              Stats &stats);
Stats runSimThread(const uint64_t &seed);
Stats runSim();