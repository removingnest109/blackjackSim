#include "simulation.h"

#include "actions.h"
#include "config.h"
#include "monitor.h"
#include <atomic>
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
  if (config.maximumBet > 0 && bet > config.maximumBet)
    bet = config.maximumBet;
  if (stats.bank < bet && !config.debtAllowed)
    return;
  turnFull(deck, dealer, rng, bet, stats);
}

// Aggregate per-player stats into a single Stats for probe publishing / return.
// Shoe-level fields (shuffles, cardsDealt, cardsSinceShuffle, runningCount,
// trueCount) are taken from the primary player (index 0) only.
static Stats aggregatePlayers(const std::vector<Stats> &players) {
  Stats agg;
  for (const auto &p : players) {
    agg.hands           += p.hands;
    agg.playerWins      += p.playerWins;
    agg.dealerWins      += p.dealerWins;
    agg.playerBlackjacks += p.playerBlackjacks;
    agg.dealerBlackjacks += p.dealerBlackjacks;
    agg.draw            += p.draw;
    agg.splits          += p.splits;
    agg.doubles         += p.doubles;
    agg.totalBet        += p.totalBet;
    agg.bank            += p.bank;
    agg.cardsDealt      += p.cardsDealt;
    agg.shuffles        += p.shuffles;
  }
  agg.cardsSinceShuffle = players[0].cardsSinceShuffle;
  agg.runningCount      = players[0].runningCount;
  agg.trueCount         = players[0].trueCount;
  return agg;
}

// Hands between stop-request polls. Power of two minus one so the test is a
// bitmask; i == 0 hits on the first iteration, so a table claimed after Stop
// was pressed exits straight away.
const int kStopCheckMask = 1023;

Stats runSimThread(const uint64_t &seed, ThreadProbe *probe,
                   SimMonitor *monitor) {
  const int N = std::max(1, config.playersPerTable);
  std::vector<Stats> players(static_cast<size_t>(N));
  for (auto &p : players)
    p.bank = config.startingBank;

  std::mt19937 rng(seed);

  std::vector<int> deck;
  shuffleDeck(deck, rng, players[0]);

  Hand dealer;

  const int interval = config.numberHands > kMaxSamples
                           ? config.numberHands / kMaxSamples
                           : 1;
  int sinceProbe = 0;

  for (int i = 0; i < config.numberHands; ++i) {
    for (int p = 0; p < N; ++p) {
      if (p > 0) {
        // Borrow shoe state from the primary player before each non-primary turn.
        players[p].cardsSinceShuffle = players[0].cardsSinceShuffle;
        players[p].runningCount      = players[0].runningCount;
        players[p].trueCount         = players[0].trueCount;
      }
      playHand(deck, dealer, rng, players[p]);
      if (p > 0) {
        // Return updated shoe state to the primary player.
        players[0].cardsSinceShuffle = players[p].cardsSinceShuffle;
        players[0].runningCount      = players[p].runningCount;
        players[0].trueCount         = players[p].trueCount;
      }
    }

    if (probe && ++sinceProbe >= interval) {
      sinceProbe = 0;
      probe->publish(aggregatePlayers(players), players);
    }

    // Polled on its own fixed cadence rather than alongside the probe: the
    // probe interval scales with numberHands (up to ~500k hands for a large
    // run), which made Stop take that long to register. The mask keeps this
    // to an increment and a compare on the hot path.
    if (monitor && (i & kStopCheckMask) == 0 &&
        monitor->stopRequested.load(std::memory_order_relaxed))
      break;
  }

  if (probe)
    probe->publish(aggregatePlayers(players), players);

  return aggregatePlayers(players);
}

Stats runSim(SimMonitor *monitor) {
  const unsigned int tables  = config.threads;
  const unsigned int hw      = std::max(1u, std::thread::hardware_concurrency());
  const unsigned int workers = std::min(tables, hw);

  std::vector<Stats> results(tables);
  // Tables left unclaimed after a stop must be excluded from the merge below,
  // otherwise their default-constructed Stats (bank 0) drag the totals down.
  // Distinct elements are written by distinct workers, so no synchronisation
  // is needed beyond the join.
  std::vector<char> ran(tables, 0);
  std::random_device dev;
  std::atomic<unsigned int> nextTable{0};

  // Seed array built upfront so workers don't race on dev().
  std::vector<uint64_t> seeds(tables);
  for (unsigned int i = 0; i < tables; ++i)
    seeds[i] = dev() + i;

  std::vector<std::thread> workerThreads;
  workerThreads.reserve(workers);
  for (unsigned int w = 0; w < workers; ++w) {
    workerThreads.emplace_back([&] {
      while (true) {
        // Check before claiming more work. Without this the pool keeps
        // dequeuing tables after Stop and only finishes once the whole queue
        // is drained, which is what made Stop appear not to take effect.
        if (monitor && monitor->stopRequested.load(std::memory_order_relaxed))
          break;
        const unsigned int i = nextTable.fetch_add(1, std::memory_order_relaxed);
        if (i >= tables)
          break;
        ThreadProbe *probe =
            monitor && i < monitor->probes.size() ? monitor->probes[i].get()
                                                  : nullptr;
        results[i] = runSimThread(seeds[i], probe, monitor);
        ran[i] = 1;
      }
    });
  }

  for (auto &t : workerThreads)
    t.join();

  Stats global{};
  for (unsigned int i = 0; i < tables; ++i)
    if (ran[i])
      global += results[i];

  return global;
}