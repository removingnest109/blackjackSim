#pragma once

#include <cstdint>
#include <random>
#include "blackjack.h"
#include "stats.h"

void playPlayerHands(Deck &deck, Hand hands[], int &handCount,
                     const Hand &dealer, Stats &stats);
void turnFull(Deck &deck, Hand &dealer, std::mt19937 &rng, const int64_t &bet,
              Stats &stats);
void playHand(Deck &deck, Hand &dealer, std::mt19937 &rng, Stats &stats);
Stats runSimThread(const uint64_t &seed);
Stats runSim();