#include "blackjack.h"
#include "config.h"
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

TEST(DetectBlackjack, SixToFivePaysReducedBonus) {
  const Config saved = config;
  config.blackjackPayout = BlackjackPayout::SixToFive;
  Hand hp, hd;
  Stats s;
  hp.cardCount = 2;
  hp.value = 21;
  hd.cardCount = 2;
  hd.value = 20;
  const std::vector<int> deck(52, 7);
  // Bet 100: original stake back plus a 6:5 bonus (120).
  EXPECT_TRUE(detectBlackjacks(hp, hd, 100, deck, s) &&
              s.playerBlackjacks > 0 && s.bank == 220);
  config = saved;
}

TEST(DetectBlackjack, SixToFiveTruncatesOddBonus) {
  const Config saved = config;
  config.blackjackPayout = BlackjackPayout::SixToFive;
  Hand hp, hd;
  Stats s;
  hp.cardCount = 2;
  hp.value = 21;
  hd.cardCount = 2;
  hd.value = 20;
  const std::vector<int> deck(52, 7);
  // Bet 5: 5 + 30/5 = 11.
  EXPECT_TRUE(detectBlackjacks(hp, hd, 5, deck, s) && s.bank == 11);
  config = saved;
}

TEST(DetectBlackjack, EvenMoneyPaysOneToOne) {
  const Config saved = config;
  config.blackjackPayout = BlackjackPayout::EvenMoney;
  Hand hp, hd;
  Stats s;
  hp.cardCount = 2;
  hp.value = 21;
  hd.cardCount = 2;
  hd.value = 20;
  const std::vector<int> deck(52, 7);
  EXPECT_TRUE(detectBlackjacks(hp, hd, 100, deck, s) &&
              s.playerBlackjacks > 0 && s.bank == 200);
  config = saved;
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