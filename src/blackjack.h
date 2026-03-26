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
bool isBlackjack(const Hand &hand);
bool detectBlackjacks(const Hand &handPlayer, const Hand &handDealer,
                      const int64_t &bet, Stats &stats);
void playDealerHand(std::vector<int> &deck, Hand &hand, Stats &stats);
void resolveHand(const Hand &player, const Hand &dealer, Stats &stats);