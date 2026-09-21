#include "actions.h"
#include "blackjack.h"
#include "config.h"
#include "simulation.h"
#include "stats.h"
#include "gtest/gtest.h"

#include <random>
#include <vector>

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

namespace {

// All-Stand chart so the deterministic round below draws no extra player
// cards. Restores the globals other tests rely on.
StrategyTable allStandChart() {
  StrategyTable t{};
  for (auto &row : t.hard)
    for (auto &c : row)
      c = Action::Stand;
  for (auto &row : t.soft)
    for (auto &c : row)
      c = Action::Stand;
  for (auto &row : t.pair)
    for (auto &c : row)
      c = Action::Stand;
  return t;
}

struct RestoreCountEnv {
  Config savedConfig = config;
  StrategyTable savedStrategy = gStrategy;
  ~RestoreCountEnv() {
    config = savedConfig;
    gStrategy = savedStrategy;
  }
};

} // namespace

// A full non-blackjack round through turnFull must count every dealt card,
// including the dealer hole (revealed before the dealer plays).
// Draw order from the back: dealer up=10, player=10, hole=5, player=6,
// dealer draw=6. Dealer 10+5=15 draws to 21; player 16 stands.
TEST(CardCounting, TurnFullCountsHoleCard) {
  RestoreCountEnv restore;
  config.cardCounting = true;
  config.surrenderAllowed = false;
  config.earlySurrender = false;
  gStrategy = allStandChart();

  std::vector<int> deck(47, 7); // neutral filler, never drawn
  deck.push_back(6);            // dealer draw
  deck.push_back(6);            // player second card
  deck.push_back(5);            // dealer hole
  deck.push_back(10);           // player first card
  deck.push_back(10);           // dealer upcard
  Hand dealer;
  std::mt19937 rng(1);
  Stats s;

  turnFull(deck, dealer, rng, 100, s);

  ASSERT_EQ(deck.size(), 47u); // exactly the 5 scripted cards were dealt
  const std::vector<int> dealt{10, 10, 5, 6, 6};
  int expected = 0;
  for (const int c : dealt)
    expected += countTable[c];
  EXPECT_EQ(expected, 1);
  EXPECT_EQ(s.runningCount, expected);
}

// trueCount must reflect the running count and remaining decks at the end of
// the hand, not the stale value from before the deal.
TEST(CardCounting, TrueCountFreshAtEndOfHand) {
  RestoreCountEnv restore;
  config.cardCounting = true;
  config.surrenderAllowed = false;
  config.earlySurrender = false;
  gStrategy = allStandChart();

  std::vector<int> deck(47, 7);
  deck.push_back(6);
  deck.push_back(6);
  deck.push_back(5);
  deck.push_back(10);
  deck.push_back(10);
  Hand dealer;
  std::mt19937 rng(1);
  Stats s;

  turnFull(deck, dealer, rng, 100, s);

  ASSERT_EQ(deck.size(), 47u);
  EXPECT_DOUBLE_EQ(s.trueCount,
                   static_cast<double>(s.runningCount) / (deck.size() / 52.0));
}