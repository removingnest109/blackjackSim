#pragma once

#include "stats.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

// Live-observation support for GUI (or other) frontends.
// A SimMonitor is optional: when none is supplied, the simulation
// runs exactly as before with zero overhead.

enum { kMaxSamples = 4096 };

struct ThreadProbe {
  // Decimated bank history, written by exactly one sim thread.
  // Readers load sampleCount with acquire ordering and may then
  // safely read the first sampleCount entries (entries are never
  // mutated after publication).
  int64_t sampleHands[kMaxSamples];
  int64_t sampleBank[kMaxSamples];
  std::atomic<int> sampleCount;

  // Latest full stats snapshot for live display.
  std::mutex latestMutex;
  Stats latest;

  ThreadProbe() : sampleCount(0) {}

  void publish(const Stats &s) {
    const int c = sampleCount.load(std::memory_order_relaxed);
    if (c < kMaxSamples) {
      sampleHands[c] = s.hands;
      sampleBank[c] = s.bank;
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
  std::atomic<bool> stopRequested;
  std::vector<std::unique_ptr<ThreadProbe>> probes;

  explicit SimMonitor(unsigned int threads) : stopRequested(false) {
    probes.reserve(threads);
    for (unsigned int i = 0; i < threads; ++i)
      probes.emplace_back(new ThreadProbe());
  }
};
