#include "blackjack.h"
#include "stats.h"
#include <gtest/gtest.h>

#include "config.h"

TEST(CardCounting, UsesExpectedThresholds) {
    Stats s;

    s.trueCount = -1.0;
    EXPECT_EQ(betFromTrueCount(s), 1);

    s.trueCount = 2.0;
    EXPECT_EQ(betFromTrueCount(s), 3);

    s.trueCount = 3.5;
    EXPECT_EQ(betFromTrueCount(s), 10);

    s.trueCount = 6.0;
    EXPECT_EQ(betFromTrueCount(s), 16);
}

TEST(CardCounting, HiLoCountsCardsCorrectly) {
    std::vector<int> d;
    Hand h;
    Stats s;
    config.cardCounting = true;
    d.push_back(2);
    drawCard(d, h,true, s);
    EXPECT_EQ(s.runningCount, 1);
    d.push_back(3);
    drawCard(d, h,true, s);
    EXPECT_EQ(s.runningCount, 2);
    d.push_back(4);
    drawCard(d, h,true, s);
    EXPECT_EQ(s.runningCount, 3);
    d.push_back(5);
    drawCard(d, h,true, s);
    EXPECT_EQ(s.runningCount, 4);
    d.push_back(6);
    drawCard(d, h,true, s);
    EXPECT_EQ(s.runningCount, 5);
    d.push_back(7);
    drawCard(d, h,true, s);
    EXPECT_EQ(s.runningCount, 5);
    d.push_back(8);
    drawCard(d, h,true, s);
    EXPECT_EQ(s.runningCount, 5);
    d.push_back(9);
    drawCard(d, h,true, s);
    EXPECT_EQ(s.runningCount, 5);
    d.push_back(10);
    drawCard(d, h,true, s);
    EXPECT_EQ(s.runningCount, 4);
    d.push_back(11);
    drawCard(d, h,true, s);
    EXPECT_EQ(s.runningCount, 3);
}

TEST(Blackjack, DetectsNaturalBlackjackOnly) {
    Hand h;
    h.cardCount = 2;
    h.value = 21;
    h.splitAces = false;
    EXPECT_TRUE(isBlackjack(h));

    h.splitAces = true;
    EXPECT_FALSE(isBlackjack(h));
}

TEST(Blackjack, DetectsBothBlackjacksWithBetReturned) {
    Hand hp, hd;
    Stats s;
    hp.cardCount = 2;
    hp.value = 21;
    hd.cardCount = 2;
    hd.value = 21;
    EXPECT_TRUE(detectBlackjacks(hp, hd, 100, s) && s.draw > 0 && s.bank == 100);
}

TEST(Blackjack, DetectsDealerBlackjack) {
    Hand hp, hd;
    Stats s;
    hp.cardCount = 2;
    hp.value = 20;
    hd.cardCount = 2;
    hd.value = 21;
    EXPECT_TRUE(detectBlackjacks(hp, hd, 100, s) && s.dealerBlackjacks > 0 && s.bank == 0);
}

TEST(Blackjack, DetectsPlayerBlackjackWithPayout) {
    Hand hp, hd;
    Stats s;
    hp.cardCount = 2;
    hp.value = 21;
    hd.cardCount = 2;
    hd.value = 20;
    EXPECT_TRUE(detectBlackjacks(hp, hd, 100, s) && s.playerBlackjacks > 0 && s.bank == 250);
}

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