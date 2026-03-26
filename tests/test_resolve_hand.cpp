#include "blackjack.h"
#include "model.h"
#include "stats.h"
#include "gtest/gtest.h"

TEST(ResolveHand, ResolvesDrawCorrectly) {
  Hand hp, hd;
  Stats s;
  hp.cardCount = 2;
  hp.value = 20;
  hd.cardCount = 2;
  hd.value = 20;
  resolveHand(hp, hd, s);
  EXPECT_TRUE(s.draw > 0);
}

TEST(ResolveHand, ResolvesPlayerWinCorrectly) {
  Hand hp, hd;
  Stats s;
  hp.cardCount = 3;
  hp.value = 20;
  hd.cardCount = 2;
  hd.value = 19;
  resolveHand(hp, hd, s);
  EXPECT_TRUE(s.playerWins > 0);
}

TEST(ResolveHand, ResolvesPlayerBustCorrectly) {
  Hand hp, hd;
  Stats s;
  hp.cardCount = 3;
  hp.value = 22;
  hd.cardCount = 2;
  hd.value = 20;
  resolveHand(hp, hd, s);
  EXPECT_TRUE(s.dealerWins > 0);
}

TEST(ResolveHand, ResolvesBothBustCorrectly) {
  Hand hp, hd;
  Stats s;
  hp.cardCount = 3;
  hp.value = 22;
  hd.cardCount = 3;
  hd.value = 22;
  resolveHand(hp, hd, s);
  EXPECT_TRUE(s.dealerWins > 0);
}

TEST(ResolveHand, ResolvesDealerWinCorrectly) {
  Hand hp, hd;
  Stats s;
  hp.cardCount = 3;
  hp.value = 19;
  hd.cardCount = 3;
  hd.value = 20;
  resolveHand(hp, hd, s);
  EXPECT_TRUE(s.dealerWins > 0);
}

TEST(ResolveHand, ResolvesDealerBustCorrectly) {
  Hand hp, hd;
  Stats s;
  hp.cardCount = 2;
  hp.value = 20;
  hd.cardCount = 3;
  hd.value = 22;
  resolveHand(hp, hd, s);
  EXPECT_TRUE(s.playerWins > 0);
}