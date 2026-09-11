#include "print.h"
#include "config.h"
#include "model.h"
#include "stats.h"
#include <format>
#include <iostream>

static double divide(const int64_t numerator, const int64_t denominator) {
  return denominator == 0 ? 0.0
                          : static_cast<double>(numerator) /
                                static_cast<double>(denominator);
}

void printGlobalVars() {
  std::cout << "SETTINGS\n";
  std::cout << std::format("Multithreading: {}\n",
                           config.multiThread ? "Enabled" : "Disabled");
  if (config.multiThread)
    std::cout << std::format("Number of threads: {}\n", config.threads);
  std::cout << std::format("Number of hands per thread: {}\n",
                           config.numberHands);
  std::cout << std::format("Starting bank: {}\n", config.startingBank);
  if (config.betPercentMode)
    std::cout << std::format("Bet size: {:g}% of current bank\n",
                             config.betPercent);
  else
    std::cout << std::format("Default bet size: {}\n", config.defaultBetSize);
  std::cout << std::format("Minimum bet: {}\n", config.minimumBet);
  std::cout << std::format("Number of decks: {}\n", config.numberDecks);
  std::cout << std::format("Penetration before shuffle: {:g}%\n",
                           config.penetrationBeforeShuffle * 100);
  std::cout << std::format("Dealer {} on soft 17\n",
                           config.dealerHitSoft17 ? "hits" : "stands");
  std::cout << std::format("Surrender: {}\n",
                           !config.surrenderAllowed ? "Disabled"
                           : config.earlySurrender  ? "Early"
                                                    : "Late");
  std::cout << std::format("Card counting: {}\n",
                           config.cardCounting ? "Enabled" : "Disabled");
  std::cout << std::format("Negative bank: {}\n",
                           config.debtAllowed ? "Enabled" : "Disabled");
  std::cout << "\n";
}

void printStats(const Stats &stats) {
  const int64_t profit =
      stats.bank - (config.startingBank * static_cast<int64_t>(config.threads));
  const auto evPercent = divide(profit, stats.totalBet);
  if (config.verbose) {
    const double evPerHand = divide(profit, stats.hands);
    const double winPercent = divide(stats.playerWins, stats.hands);
    std::cout << "RESULTS\n";
    std::cout << std::format("{} Hands played\n", stats.hands);
    std::cout << std::format("{} Dealer Wins\n", stats.dealerWins);
    std::cout << std::format("{} Dealer Blackjacks\n", stats.dealerBlackjacks);
    std::cout << std::format("{} Draw\n", stats.draw);
    std::cout << std::format("{} Player Wins\n", stats.playerWins);
    std::cout << std::format("{} Player Blackjacks\n", stats.playerBlackjacks);
    std::cout << std::format("{} Shuffles\n", stats.shuffles);
    std::cout << std::format("{} Cards dealt\n", stats.cardsDealt);
    std::cout << std::format("{} Splits\n", stats.splits);
    std::cout << std::format("{} Doubles\n", stats.doubles);
    std::cout << std::format("{} Surrenders\n", stats.surrenders);
    std::cout << std::format("Average player win percentage: {:g}%\n",
                             winPercent * 100);
    std::cout << std::format(
        "Average player bank: {:g}\n",
        divide(stats.bank, static_cast<int64_t>(config.threads)));
    std::cout << std::format(
        "Average profit: {:g}\n",
        divide(profit, static_cast<int64_t>(config.threads)));
    std::cout << std::format("Average EV per hand: {:g} $\n", evPerHand);
  }
  std::cout << std::format("Average EV percentage: {:g}%\n", evPercent * 100);
}
