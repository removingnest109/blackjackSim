#include "blackjack.h"
#include "config.h"
#include "stats.h"
#include "gtest/gtest.h"

TEST(CardCounting, UsesExpectedThresholds) {
  Stats s;

  s.trueCount = -1.0;
  EXPECT_EQ(betFromTrueCount(s), config.betCurve[0]);

  s.trueCount = 2.0;
  EXPECT_EQ(betFromTrueCount(s), config.betCurve[1]);

  s.trueCount = 3.5;
  EXPECT_EQ(betFromTrueCount(s), config.betCurve[3]);

  s.trueCount = 6.0;
  EXPECT_EQ(betFromTrueCount(s), config.betCurve[5]);
}

TEST(CardCounting, BetCurveIsConfigurable) {
  config = Config();
  config.betCurve[0] = 2;
  config.betCurve[1] = 7;
  config.betCurve[5] = 50;
  Stats s;

  s.trueCount = -1.0;
  EXPECT_EQ(betFromTrueCount(s), 2);
  s.trueCount = 1.5;
  EXPECT_EQ(betFromTrueCount(s), 7);
  s.trueCount = 9.0;
  EXPECT_EQ(betFromTrueCount(s), 50);

  config = Config();
}

TEST(CardCounting, HiLoCountsCardsCorrectly) {
  std::vector<int> d;
  Hand h;
  Stats s;
  config.cardCounting = true;
  d.push_back(2);
  drawCard(d, h, true, s);
  EXPECT_EQ(s.runningCount, 1);
  d.push_back(3);
  drawCard(d, h, true, s);
  EXPECT_EQ(s.runningCount, 2);
  d.push_back(4);
  drawCard(d, h, true, s);
  EXPECT_EQ(s.runningCount, 3);
  d.push_back(5);
  drawCard(d, h, true, s);
  EXPECT_EQ(s.runningCount, 4);
  d.push_back(6);
  drawCard(d, h, true, s);
  EXPECT_EQ(s.runningCount, 5);
  d.push_back(7);
  drawCard(d, h, true, s);
  EXPECT_EQ(s.runningCount, 5);
  d.push_back(8);
  drawCard(d, h, true, s);
  EXPECT_EQ(s.runningCount, 5);
  d.push_back(9);
  drawCard(d, h, true, s);
  EXPECT_EQ(s.runningCount, 5);
  d.push_back(10);
  drawCard(d, h, true, s);
  EXPECT_EQ(s.runningCount, 4);
  d.push_back(11);
  drawCard(d, h, true, s);
  EXPECT_EQ(s.runningCount, 3);
}