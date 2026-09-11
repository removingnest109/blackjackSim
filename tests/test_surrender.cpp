#include "actions.h"
#include "config.h"
#include "model.h"
#include "simulation.h"
#include "stats.h"
#include "gtest/gtest.h"

#include <random>
#include <vector>

namespace {

// A chart where every cell is Stand, so any hand that isn't surrendered stops
// immediately and the tests stay deterministic. Callers flip the one cell they
// exercise to Surrender.
StrategyTable allStand() {
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

// Restores global state other tests rely on.
struct Restore {
  ~Restore() {
    gStrategy = kBasicStrategy;
    config.surrenderAllowed = false;
    config.earlySurrender = false;
  }
};

Hand makeTwoCard(int a, int b, int64_t bet) {
  Hand h;
  h.cards[0] = a;
  h.cards[1] = b;
  h.cardCount = 2;
  h.value = a + b;
  h.aceCount = (a == 11) + (b == 11);
  h.bet = bet;
  return h;
}

} // namespace

// A fresh, unsplit 2-card hand on a Surrender cell loses exactly half its bet
// and increments the surrenders stat, with no further cards drawn.
TEST(Surrender, LateSurrenderRefundsHalfBet) {
  Restore restore;
  gStrategy = allStand();
  gStrategy.hard[16][10] = Action::Surrender;
  config.surrenderAllowed = true;
  config.earlySurrender = false;

  std::vector<int> deck{5, 5, 5, 5};
  Hand dealer;
  dealer.cards[0] = 10;
  std::vector<Hand> hands{makeTwoCard(10, 6, 100)};
  hands.resize(4);
  hands[0] = makeTwoCard(10, 6, 100);
  int handCount = 1;
  Stats s;
  s.bank = 0;

  simulatePlayerHands(deck, hands, handCount, dealer, s);

  EXPECT_EQ(s.surrenders, 1);
  EXPECT_EQ(s.bank, 50);           // 0 + 100/2
  EXPECT_EQ(hands[0].cardCount, 2); // no extra card drawn
}

// After a split (handCount > 1) the same cell is illegal and degrades to Hit.
TEST(Surrender, FallsBackToHitAfterSplit) {
  Restore restore;
  gStrategy = allStand();
  gStrategy.hard[16][10] = Action::Surrender;
  config.surrenderAllowed = true;
  config.earlySurrender = false;

  std::vector<int> deck{5, 5, 5, 5};
  Hand dealer;
  dealer.cards[0] = 10;
  std::vector<Hand> hands(4);
  hands[0] = makeTwoCard(10, 6, 100);
  hands[1] = makeTwoCard(10, 9, 100); // second split hand, stands at 19
  int handCount = 2;
  Stats s;
  s.bank = 0;

  simulatePlayerHands(deck, hands, handCount, dealer, s);

  EXPECT_EQ(s.surrenders, 0);
  EXPECT_EQ(s.bank, 0);              // no surrender refund
  EXPECT_EQ(hands[0].cardCount, 3); // drew a card instead (fallback Hit)
}

// With surrender disabled, a Surrender cell also degrades to Hit.
TEST(Surrender, FallsBackToHitWhenDisabled) {
  Restore restore;
  gStrategy = allStand();
  gStrategy.hard[16][10] = Action::Surrender;
  config.surrenderAllowed = false;

  std::vector<int> deck{5, 5, 5, 5};
  Hand dealer;
  dealer.cards[0] = 10;
  std::vector<Hand> hands(4);
  hands[0] = makeTwoCard(10, 6, 100);
  int handCount = 1;
  Stats s;
  s.bank = 0;

  simulatePlayerHands(deck, hands, handCount, dealer, s);

  EXPECT_EQ(s.surrenders, 0);
  EXPECT_EQ(hands[0].cardCount, 3); // fallback Hit
}

// Early surrender resolves before the dealer peek, so it escapes a dealer
// blackjack: the player loses only half the bet and the dealer BJ is never
// recorded.
TEST(Surrender, EarlyEscapesDealerBlackjack) {
  Restore restore;
  gStrategy = allStand();
  gStrategy.hard[16][11] = Action::Surrender; // vs dealer Ace upcard
  config.surrenderAllowed = true;
  config.earlySurrender = true;

  // Draw order (from back): dealer up=11, player=10, dealer hole=10, player=6.
  std::vector<int> deck{6, 10, 10, 11};
  Hand dealer;
  std::mt19937 rng(1);
  Stats s;
  s.bank = 0;

  turnFull(deck, dealer, rng, 100, s);

  EXPECT_EQ(s.surrenders, 1);
  EXPECT_EQ(s.dealerBlackjacks, 0); // escaped the peek
  EXPECT_EQ(s.bank, -50);           // -100 deducted + 50 refund
}

// The same deal under late surrender does NOT escape: the dealer blackjack is
// detected first and the player loses the full bet.
TEST(Surrender, LateDoesNotEscapeDealerBlackjack) {
  Restore restore;
  gStrategy = allStand();
  gStrategy.hard[16][11] = Action::Surrender;
  config.surrenderAllowed = true;
  config.earlySurrender = false;

  std::vector<int> deck{6, 10, 10, 11};
  Hand dealer;
  std::mt19937 rng(1);
  Stats s;
  s.bank = 0;

  turnFull(deck, dealer, rng, 100, s);

  EXPECT_EQ(s.surrenders, 0);
  EXPECT_EQ(s.dealerBlackjacks, 1);
  EXPECT_EQ(s.bank, -100); // full-bet loss to dealer BJ
}
