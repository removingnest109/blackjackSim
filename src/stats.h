#pragma once
#include <cstdint>

struct Stats {
  int64_t hands = 0;
  int64_t playerWins = 0;
  int64_t dealerWins = 0;
  int64_t playerBlackjacks = 0;
  int64_t dealerBlackjacks = 0;
  int64_t draw = 0;
  int64_t shuffles = 0;
  int64_t cardsDealt = 0;
  int64_t splits = 0;
  int64_t doubles = 0;
  int64_t totalBet = 0;
  int64_t bank = 0;
  int64_t cardsSinceShuffle = 0;
  int64_t runningCount = 0;
  double trueCount = 0;

  Stats &operator+=(const Stats &o) {
    hands += o.hands;
    playerWins += o.playerWins;
    dealerWins += o.dealerWins;
    playerBlackjacks += o.playerBlackjacks;
    dealerBlackjacks += o.dealerBlackjacks;
    draw += o.draw;
    shuffles += o.shuffles;
    cardsDealt += o.cardsDealt;
    splits += o.splits;
    doubles += o.doubles;
    totalBet += o.totalBet;
    bank += o.bank;
    return *this;
  }
};
