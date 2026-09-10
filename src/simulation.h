#pragma once

#include "blackjack.h"
#include "stats.h"
#include <cstdint>
#include <random>
#include <span>
#include <stop_token>

void simulatePlayerHands(std::vector<int> &deck, std::span<Hand> hands,
                         int &handCount, const Hand &dealer, Stats &stats);
void turnFull(std::vector<int> &deck, Hand &dealer, std::mt19937 &rng,
              const int64_t &bet, Stats &stats);
void playHand(std::vector<int> &deck, Hand &dealer, std::mt19937 &rng,
              Stats &stats);
struct ThreadProbe;
struct SimMonitor;

Stats runSimThread(const uint64_t &seed, ThreadProbe *probe = nullptr,
                   std::stop_token stop = {});
Stats runSim(SimMonitor *monitor = nullptr);