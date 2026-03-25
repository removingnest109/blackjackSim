#include "print.h"
#include "config.h"
#include "model.h"
#include "stats.h"
#include <iostream>

static double divide(const int64_t numerator, const int64_t denominator) {
  return denominator == 0 ? 0.0
                          : static_cast<double>(numerator) /
                                static_cast<double>(denominator);
}

void printGlobalVars() {
  std::cout << "SETTINGS" << std::endl;
  std::cout << "Multithreading: " << (config.multiThread ? "Enabled" : "Disabled")
            << std::endl;
  if (config.multiThread)
    std::cout << "Number of threads: " << config.threads << std::endl;
  std::cout << "Number of hands per thread: " << config.numberHands << std::endl;
  std::cout << "Starting bank: " << config.startingBank << std::endl;
  std::cout << "Default bet size: " << config.defaultBetSize << std::endl;
  std::cout << "Number of decks: " << config.numberDecks << std::endl;
  std::cout << "Penetration before shuffle: "
            << config.penetrationBeforeShuffle * 100 << "%" << std::endl;
  std::cout << "Dealer " << (config.dealerHitSoft17 ? "hits" : "stands")
            << " on soft 17" << std::endl;
  std::cout << "Card counting: " << (config.cardCounting ? "Enabled" : "Disabled")
            << std::endl;
  std::cout << "Negative bank: " << (config.debtAllowed ? "Enabled" : "Disabled")
            << std::endl;
  std::cout << std::endl;
}

void printStats(const Stats &stats) {
  const int64_t profit =
      stats.bank - (config.startingBank * static_cast<int64_t>(config.threads));
  const auto evPercent = divide(profit, stats.totalBet);
  if (config.verbose) {
    const double evPerHand = divide(profit, stats.hands);
    const double winPercent = divide(stats.playerWins, stats.hands);
    std::cout << "RESULTS" << std::endl;
    std::cout << stats.hands << " Hands played" << std::endl;
    std::cout << stats.dealerWins << " Dealer Wins" << std::endl;
    std::cout << stats.dealerBlackjacks << " Dealer Blackjacks" << std::endl;
    std::cout << stats.draw << " Draw" << std::endl;
    std::cout << stats.playerWins << " Player Wins" << std::endl;
    std::cout << stats.playerBlackjacks << " Player Blackjacks" << std::endl;
    std::cout << stats.shuffles << " Shuffles" << std::endl;
    std::cout << stats.cardsDealt << " Cards dealt" << std::endl;
    std::cout << stats.splits << " Splits" << std::endl;
    std::cout << stats.doubles << " Doubles" << std::endl;
    std::cout << "Average player win percentage: " << winPercent * 100 << "%"
              << std::endl;
    std::cout << "Average player bank: "
              << divide(stats.bank, static_cast<int64_t>(config.threads))
              << std::endl;
    std::cout << "Average profit: "
              << divide(profit, static_cast<int64_t>(config.threads)) << std::endl;
    std::cout << "Average EV per hand: " << evPerHand << " $" << std::endl;
  }
  std::cout << "Average EV percentage: " << evPercent * 100 << "%" << std::endl;
}

void announceIfInteractive(const std::string &message) {
  if (config.isInteractive)
    std::cout << message << std::endl;
}

void printHandState(const std::string &label, const Hand &hand) {
  std::cout << label << " Hand: ";
  for (int i = 0; i < hand.cardCount; ++i)
    std::cout << hand.cards[i] << " ";
  std::cout << " -> " << hand.value << std::endl;
}
