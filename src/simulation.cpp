#include "simulation.h"

#include "actions.h"
#include "config.h"
#include "monitor.h"
#include <atomic>
#include <thread>

void simulatePlayerHands(std::vector<int> &deck, std::span<Hand> hands,
                         int &handCount, const Hand &dealer, Stats &stats) {
  for (int i = 0; i < handCount; ++i) {
    bool done = false;
    while (!done) {
      Hand &hand = hands[i];
      const int dealerUp = dealer.cards[0];

      if (hand.splitAces || hand.value >= 21) {
        break;
      }

      // Doubles and splits put a second hand.bet on the table; without debt
      // they're only offered while the bank still covers that amount. An
      // uncovered pair is played as its hard/soft total instead.
      const bool canAffordExtra =
          config.debtAllowed || stats.bank >= hand.bet;
      switch (getAction(hand.value, dealerUp, hand.isSoft(),
                        hand.cardCount == 2 && hand.cards[0] == hand.cards[1] &&
                            handCount < 4 && canAffordExtra,
                        hand.cards[0])) {
      case Action::Hit:
        drawCard(deck, hand, true, stats);
        break;
      case Action::Double:
        if (!canAffordExtra) { // can't cover the second bet: hit instead
          drawCard(deck, hand, true, stats);
          break;
        }
        doubleDown(deck, hand, stats);
        done = true;
        break;
      case Action::Split:
        hands[handCount++] = split(deck, hand, stats);
        break;
      case Action::Surrender:
        // Late surrender: legal only as the first action on a fresh 2-card
        // hand that has not been split (handCount == 1 rules out a split).
        // When early surrender is on it was already resolved pre-peek in
        // turnFull, so this path must not fire again. Any illegal position
        // degrades to Hit ("R else H").
        if (config.surrenderAllowed && !config.earlySurrender &&
            hand.cardCount == 2 && handCount == 1) {
          stats.bank += hand.bet / 2; // refund half the deducted bet
          stats.surrenders++;
          done = true;
        } else {
          drawCard(deck, hand, true, stats);
        }
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

  // Early surrender bails before the dealer peek, so it escapes a dealer
  // blackjack. Resolved here, before detectBlackjacks, on the fresh 2-card
  // opening hand only.
  if (config.surrenderAllowed && config.earlySurrender) {
    const int dealerUp = dealer.cards[0];
    Hand &h = hands[0];
    const bool isPair = h.cards[0] == h.cards[1];
    if (getAction(h.value, dealerUp, h.isSoft(), isPair, h.cards[0]) ==
        Action::Surrender) {
      stats.bank += h.bet / 2; // refund half the deducted bet
      stats.surrenders++;
      return; // skip peek, player action, and resolution
    }
  }

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
  int64_t bet = computeBet(stats);
  if (!config.debtAllowed) {
    // Bankrupt only when the bank can't cover the table minimum; a player who
    // can still cover it but not their intended bet goes all-in instead.
    if (stats.bank < config.minimumBet)
      return;
    if (stats.bank < bet)
      bet = stats.bank;
  }
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
    agg.surrenders      += p.surrenders;
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
constexpr int kStopCheckMask = 1023;

Stats runSimThread(const uint64_t &seed, ThreadProbe *probe,
                   std::stop_token stop) {
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
    if ((i & kStopCheckMask) == 0 && stop.stop_requested()) [[unlikely]]
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
  // Tables left unclaimed after a stop contribute only their untouched
  // starting banks to the merge below; counting their default-constructed
  // Stats (bank 0) instead would drag the totals down. Distinct elements are
  // written by distinct workers, so no synchronisation is needed beyond the
  // join.
  std::vector<char> ran(tables, 0);
  std::random_device dev;
  std::atomic<unsigned int> nextTable{0};

  // Seed array built upfront so workers don't race on dev().
  std::vector<uint64_t> seeds(tables);
  for (unsigned int i = 0; i < tables; ++i)
    seeds[i] = dev() + i;

  // Cooperative-cancellation token shared by every worker; empty (never
  // stopped) when the caller supplied no monitor.
  const std::stop_token stop =
      monitor ? monitor->stopToken() : std::stop_token{};

  // jthreads join automatically when this scope ends, so there is no explicit
  // join loop and an exception on the merge path can't leak a running thread.
  {
    std::vector<std::jthread> workerThreads;
    workerThreads.reserve(workers);
    for (unsigned int w = 0; w < workers; ++w) {
      workerThreads.emplace_back([&] {
        while (true) {
          // Check before claiming more work. Without this the pool keeps
          // dequeuing tables after Stop and only finishes once the whole queue
          // is drained, which is what made Stop appear not to take effect.
          if (stop.stop_requested())
            break;
          const unsigned int i =
              nextTable.fetch_add(1, std::memory_order_relaxed);
          if (i >= tables)
            break;
          ThreadProbe *probe =
              monitor && i < monitor->probes.size() ? monitor->probes[i].get()
                                                    : nullptr;
          results[i] = runSimThread(seeds[i], probe, stop);
          ran[i] = 1;
        }
      });
    }
  }

  const int64_t untouchedBank =
      static_cast<int64_t>(config.startingBank) *
      std::max(1, config.playersPerTable);
  Stats global{};
  for (unsigned int i = 0; i < tables; ++i) {
    if (ran[i])
      global += results[i];
    else
      global.bank += untouchedBank;
  }

  return global;
}