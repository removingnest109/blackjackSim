#include "blackjack.h"
#include "stats.h"
#include <gtest/gtest.h>

#include <vector>

TEST(DetectBlackjack, DetectsNaturalBlackjackOnly) {
  Hand h;
  h.cardCount = 2;
  h.value = 21;
  h.splitAces = false;
  EXPECT_TRUE(isBlackjack(h));

  h.splitAces = true;
  EXPECT_FALSE(isBlackjack(h));
}

TEST(DetectBlackjack, DetectsBothBlackjacksWithBetReturned) {
  Hand hp, hd;
  Stats s;
  hp.cardCount = 2;
  hp.value = 21;
  hd.cardCount = 2;
  hd.value = 21;
  const std::vector<int> deck(52, 7);
  EXPECT_TRUE(detectBlackjacks(hp, hd, 100, deck, s) && s.draw > 0 &&
              s.bank == 100);
}

TEST(DetectBlackjack, DetectsDealerBlackjack) {
  Hand hp, hd;
  Stats s;
  hp.cardCount = 2;
  hp.value = 20;
  hd.cardCount = 2;
  hd.value = 21;
  const std::vector<int> deck(52, 7);
  EXPECT_TRUE(detectBlackjacks(hp, hd, 100, deck, s) &&
              s.dealerBlackjacks > 0 && s.bank == 0);
}

TEST(DetectBlackjack, DetectsPlayerBlackjackWithPayout) {
  Hand hp, hd;
  Stats s;
  hp.cardCount = 2;
  hp.value = 21;
  hd.cardCount = 2;
  hd.value = 20;
  const std::vector<int> deck(52, 7);
  EXPECT_TRUE(detectBlackjacks(hp, hd, 100, deck, s) &&
              s.playerBlackjacks > 0 && s.bank == 250);
}