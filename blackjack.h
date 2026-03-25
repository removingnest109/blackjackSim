#pragma once

#include <random>
#include "model.h"
#include "stats.h"

void drawCard(Deck &deck, Hand &hand, const bool &visible, Stats &stats);
void initDeck(Deck &deck);
void shuffleDeck(Deck &deck, std::mt19937 &rng, Stats &stats);
void resetHand(Hand &hand, const int64_t &bet = 0);
void shuffleIfNeeded(Deck &deck, std::mt19937 &rng, Stats &stats);
void dealInitialCards(Deck &deck, Hand &handPlayer, Hand &handDealer,
                      std::mt19937 &rng, const int64_t &bet, Stats &stats);
Hand makeHand(const int64_t &bet);
Hand split(Deck &deck, Hand &originalHand, Stats &stats);
void doubleDown(Deck &deck, Hand &hand, Stats &stats);
void getTrueCount(const Deck &deck, Stats &stats);
int64_t betFromTrueCount(const Stats &stats);
bool isBlackjack(const Hand &hand);
bool detectBlackjacks(const Hand &handPlayer, const Hand &handDealer,
                      const int64_t &bet, Stats &stats);
void playDealerHand(Deck &deck, Hand &hand, Stats &stats);
void resolveHand(const Hand &player, const Hand &dealer, Stats &stats);