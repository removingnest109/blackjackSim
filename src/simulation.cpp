#include "simulation.h"

#include "actions.h"
#include "config.h"
#include "interactive.h"
#include <iomanip>
#include <iostream>
#include <thread>

void simulatePlayerHands(std::vector<int> &deck, Hand hands[], int &handCount,
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

void playPlayerHands(std::vector<int> &deck, Hand hands[], int &handCount,
                     const Hand &dealer, Stats &stats) {
  if (config.isInteractive) {
    std::cout << "Round " << stats.hands << std::endl;
    interactiveHand(deck, hands, handCount, dealer, stats);
  } else {
    simulatePlayerHands(deck, hands, handCount, dealer, stats);
  }
}

void turnFull(std::vector<int> &deck, Hand &dealer, std::mt19937 &rng,
              const int64_t &bet, Stats &stats) {
  Hand hands[4];
  int handCount = 1;
  hands[0] = makeHand(bet);

  shuffleIfNeeded(deck, rng, stats);

  dealInitialCards(deck, hands[0], dealer, bet, stats);
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

void playHand(std::vector<int> &deck, Hand &dealer, std::mt19937 &rng,
              Stats &stats) {
  if (config.cardCounting)
    getTrueCount(deck, stats);
  int64_t bet = config.cardCounting
                    ? betFromTrueCount(stats) * config.defaultBetSize
                    : config.defaultBetSize;
  if (config.isInteractive) {
    if (config.cardCounting)
      std::cout << "count (true count): " << stats.runningCount << " ("
                << std::setprecision(2) << std::fixed << stats.trueCount << ")"
                << std::endl;
    std::cout << "bank: $" << stats.bank << std::endl;
    std::cout << "enter bet: $";
    std::cin >> bet;
  }
  if (stats.bank < bet && !config.debtAllowed)
    return;
  ;
  turnFull(deck, dealer, rng, bet, stats);
}

Stats runSimThread(const uint64_t &seed) {
  Stats local;
  local.bank = config.startingBank;
  std::mt19937 rng(seed);

  std::vector<int> deck;
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