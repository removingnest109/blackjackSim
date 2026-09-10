#pragma once

#include "stats.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <stop_token>
#include <vector>

// Live-observation support for GUI (or other) frontends.
// A SimMonitor is optional: when none is supplied, the simulation
// runs exactly as before with zero overhead.

inline constexpr int kMaxSamples = 4096;

struct ThreadProbe {
  int playerCount;

  // Decimated bank history, written by exactly one sim thread.
  // Readers load sampleCount with acquire ordering and may then
  // safely read the first sampleCount entries (entries are never
  // mutated after publication).
  int64_t sampleHands[kMaxSamples];
  int64_t sampleBank[kMaxSamples]; // aggregate across all players in thread
  // Per-player bank history, flat layout: [sample * playerCount + player].
  std::vector<int64_t> samplePlayerBanks;
  std::atomic<int> sampleCount;

  // Latest full stats snapshot for live display.
  std::mutex latestMutex;
  Stats latest;

  // Until the first publish, `latest` reports the table's combined starting
  // bank: players at a table that hasn't been claimed by a worker yet still
  // hold their full starting bank, and readers summing readLatest() across
  // probes would otherwise see a phantom loss whenever tables > workers.
  explicit ThreadProbe(int players = 1, int64_t startingBank = 0)
      : playerCount(players), sampleCount(0),
        samplePlayerBanks(static_cast<size_t>(kMaxSamples) * players, 0) {
    latest.bank = startingBank * players;
  }

  // Publish aggregate stats together with per-player bank values.
  void publish(const Stats &agg, const std::vector<Stats> &perPlayer) {
    const int c = sampleCount.load(std::memory_order_relaxed);
    if (c < kMaxSamples) [[likely]] {
      sampleHands[c] = perPlayer.empty() ? agg.hands : perPlayer[0].hands;
      sampleBank[c] = agg.bank;
      for (int p = 0; p < playerCount; ++p) {
        samplePlayerBanks[static_cast<size_t>(c) * playerCount + p] =
            (p < static_cast<int>(perPlayer.size())) ? perPlayer[p].bank
                                                     : agg.bank;
      }
      sampleCount.store(c + 1, std::memory_order_release);
    }
    std::lock_guard<std::mutex> lock(latestMutex);
    latest = agg;
  }

  // Single-stats overload for backward compatibility.
  void publish(const Stats &s) {
    const int c = sampleCount.load(std::memory_order_relaxed);
    if (c < kMaxSamples) {
      sampleHands[c] = s.hands;
      sampleBank[c] = s.bank;
      samplePlayerBanks[static_cast<size_t>(c) * playerCount] = s.bank;
      sampleCount.store(c + 1, std::memory_order_release);
    }
    std::lock_guard<std::mutex> lock(latestMutex);
    latest = s;
  }

  Stats readLatest() {
    std::lock_guard<std::mutex> lock(latestMutex);
    return latest;
  }
};

struct SimMonitor {
  // Cooperative cancellation for the worker pool. Frontends call requestStop();
  // workers poll the token via stopToken(). Replaces the hand-rolled
  // atomic<bool> flag with the standard C++20 stop mechanism.
  std::stop_source stopSource;
  std::vector<std::unique_ptr<ThreadProbe>> probes;

  explicit SimMonitor(unsigned int threads, int playersPerTable = 1,
                      int64_t startingBank = 0) {
    probes.reserve(threads);
    for (unsigned int i = 0; i < threads; ++i)
      probes.push_back(
          std::make_unique<ThreadProbe>(playersPerTable, startingBank));
  }

  void requestStop() { stopSource.request_stop(); }
  std::stop_token stopToken() const { return stopSource.get_token(); }
};
