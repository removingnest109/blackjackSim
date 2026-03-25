#include "simulation.h"

#include <iomanip>
#include <iostream>
#include <thread>
#include "actions.h"
#include "config.h"
#include "interactive.h"

void simulatePlayerHands(Deck &deck, Hand hands[], int &handCount,
                         const Hand &dealer, Stats &stats) {
  for (int i = 0; i < handCount; ++i) {
    bool done = false;
    while (!done) {
      Hand &hand = hands[i];
      const int dealerUp = dealer.cards[0];

      if (hand.splitAces || hand.value >= 21) {
        break;
      }

      switch (getAction(hand.value, dealerUp, hand.isSoft(),
                        hand.cardCount == 2 && hand.cards[0] == hand.cards[1] &&
                            handCount < 4,
                        hand.cards[0])) {
      case Action::Hit:
        drawCard(deck, hand, true, stats);
        break;
      case Action::Double:
        doubleDown(deck, hand, stats);
        done = true;
        break;
      case Action::Split:
        hands[handCount++] = split(deck, hand, stats);
        break;
      case Action::Stand:
      default:
        done = true;
        break;
      }
    }
  }
}

void playPlayerHands(Deck &deck, Hand hands[], int &handCount,
                     const Hand &dealer, Stats &stats) {
  if (config.isInteractive) {
    std::cout << "Round " << stats.hands << std::endl;
    interactiveHand(deck, hands, handCount, dealer, stats);
  } else {
    simulatePlayerHands(deck, hands, handCount, dealer, stats);
  }
}



void turnFull(Deck &deck, Hand &dealer, std::mt19937 &rng, const int64_t &bet,
              Stats &stats) {
  Hand hands[4];
  int handCount = 1;
  hands[0] = makeHand(bet);

  dealInitialCards(deck, hands[0], dealer, rng, bet, stats);
  stats.hands++;

  if (detectBlackjacks(hands[0], dealer, bet, stats))
    return;

  playPlayerHands(deck, hands, handCount, dealer, stats);
  playDealerHand(deck, dealer, stats);

  for (int i = 0; i < handCount; ++i) {
    if (config.isInteractive)
      std::cout << "Hand " << (i + 1) << std::endl;
    resolveHand(hands[i], dealer, stats);
  }
}

void playHand(Deck &deck, Hand &dealer, std::mt19937 &rng, Stats &local) {
  if (config.cardCounting)
    getTrueCount(deck, local);
  int64_t bet = config.cardCounting
                    ? betFromTrueCount(local) * config.defaultBetSize
                    : config.defaultBetSize;
  if (config.isInteractive) {
    if (config.cardCounting)
      std::cout << "count (true count): " << local.runningCount << " ("
                << std::setprecision(2) << std::fixed << local.trueCount << ")"
                << std::endl;
    std::cout << "bank: $" << local.bank << std::endl;
    std::cout << "enter bet: $";
    std::cin >> bet;
  }
  if (local.bank < bet && !config.debtAllowed)
    return;
  ;
  turnFull(deck, dealer, rng, bet, local);
}

Stats runSimThread(const uint64_t &seed) {
  Stats local;
  local.bank = config.startingBank;
  std::mt19937 rng(seed);

  Deck deck;
  shuffleDeck(deck, rng, local);

  Hand dealer;

  for (int i = 0; i < config.numberHands; ++i) {
    playHand(deck, dealer, rng, local);
  }

  return local;
}

Stats runSim() {
  std::vector<std::thread> workers;
  std::vector<Stats> results(config.threads);
  std::random_device dev;

  workers.reserve(config.threads);
  for (unsigned int i = 0; i < config.threads; ++i) {
    workers.emplace_back([&, i] { results[i] = runSimThread(dev() + i); });
  }

  for (auto &t : workers)
    t.join();

  Stats global{};
  for (const auto &s : results) {
    global += s;
  }

  return global;
}