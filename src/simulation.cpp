#include "simulation.h"

#include "actions.h"
#include "config.h"
#include "monitor.h"
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

  simulatePlayerHands(deck, hands, handCount, dealer, stats);
  playDealerHand(deck, dealer, stats);

  for (int i = 0; i < handCount; ++i) {
    resolveHand(hands[i], dealer, stats);
  }
}

void playHand(std::vector<int> &deck, Hand &dealer, std::mt19937 &rng,
              Stats &stats) {
  if (config.cardCounting)
    getTrueCount(deck, stats);
  int64_t baseBet = config.defaultBetSize;
  if (config.betPercentMode)
    baseBet = static_cast<int64_t>(static_cast<double>(stats.bank) *
                                   (config.betPercent / 100.0));
  int64_t bet =
      config.cardCounting ? betFromTrueCount(stats) * baseBet : baseBet;
  if (bet < config.minimumBet)
    bet = config.minimumBet;
  if (stats.bank < bet && !config.debtAllowed)
    return;
  turnFull(deck, dealer, rng, bet, stats);
}

Stats runSimThread(const uint64_t &seed, ThreadProbe *probe,
                   SimMonitor *monitor) {
  Stats local;
  local.bank = config.startingBank;
  std::mt19937 rng(seed);

  std::vector<int> deck;
  shuffleDeck(deck, rng, local);

  Hand dealer;

  const int interval = config.numberHands > kMaxSamples
                           ? config.numberHands / kMaxSamples
                           : 1;
  int sinceProbe = 0;

  for (int i = 0; i < config.numberHands; ++i) {
    playHand(deck, dealer, rng, local);
    if (probe && ++sinceProbe >= interval) {
      sinceProbe = 0;
      probe->publish(local);
      if (monitor &&
          monitor->stopRequested.load(std::memory_order_relaxed))
        break;
    }
  }

  if (probe)
    probe->publish(local);

  return local;
}

Stats runSim(SimMonitor *monitor) {
  std::vector<std::thread> workers;
  std::vector<Stats> results(config.threads);
  std::random_device dev;

  workers.reserve(config.threads);
  for (unsigned int i = 0; i < config.threads; ++i) {
    ThreadProbe *probe =
        monitor && i < monitor->probes.size() ? monitor->probes[i].get()
                                              : nullptr;
    workers.emplace_back(
        [&, i, probe] { results[i] = runSimThread(dev() + i, probe, monitor); });
  }

  for (auto &t : workers)
    t.join();

  Stats global{};
  for (const auto &s : results) {
    global += s;
  }

  return global;
}