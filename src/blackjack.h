#pragma once

#include "model.h"
#include "stats.h"
#include <random>

void drawCard(std::vector<int> &deck, Hand &hand, const bool &visible,
              Stats &stats);
void initDeck(std::vector<int> &deck);
void shuffleDeck(std::vector<int> &deck, std::mt19937 &rng, Stats &stats);
void resetHand(Hand &hand, const int64_t &bet = 0);
void shuffleIfNeeded(std::vector<int> &deck, std::mt19937 &rng, Stats &stats);
void dealInitialCards(std::vector<int> &deck, Hand &handPlayer,
                      Hand &handDealer, const int64_t &bet, Stats &stats);
Hand makeHand(const int64_t &bet);
Hand split(std::vector<int> &deck, Hand &originalHand, Stats &stats);
void doubleDown(std::vector<int> &deck, Hand &hand, Stats &stats);
void getTrueCount(const std::vector<int> &deck, Stats &stats);
int64_t betFromTrueCount(const Stats &stats);
// Intended wager for the next hand from the current bank/count and config:
// applies percent-of-bank or flat sizing, the count multiplier, then the
// minimum/maximum clamps. Pure function of stats + config (no RNG); the
// bankrupt sit-out and all-in adjustments stay in playHand.
int64_t computeBet(const Stats &stats);
bool isBlackjack(const Hand &hand);
bool detectBlackjacks(const Hand &handPlayer, const Hand &handDealer,
                      const int64_t &bet,
                      const std::vector<int> &deck, Stats &stats);
void playDealerHand(std::vector<int> &deck, Hand &hand, Stats &stats);
void resolveHand(const Hand &player, const Hand &dealer, Stats &stats);