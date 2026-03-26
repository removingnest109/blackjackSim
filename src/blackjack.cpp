#include "blackjack.h"
#include "config.h"
#include "print.h"
#include <algorithm>

void drawCard(std::vector<int> &deck, Hand &hand, const bool &visible, Stats &stats) {
  const int card = deck.back();
  deck.pop_back();

  stats.cardsSinceShuffle++;
  stats.cardsDealt++;

  if (config.cardCounting) {
    stats.runningCount += visible * countTable[card];
  }

  hand.cards[hand.cardCount++] = card;
  hand.value += card;
  if (card == 11)
    hand.aceCount++;

  while (hand.value > 21 && hand.aceCount > 0) {
    hand.value -= 10;
    hand.aceCount--;
  }
}

void initDeck(std::vector<int> &deck) {
  deck.clear();
  deck.reserve(config.numberDecks * 52);

  for (int d = 0; d < config.numberDecks; ++d) { // do once per deck
    for (int value = 2; value <= 10; ++value) {  // for each value 2-10
      for (int count = 0; count < 4; ++count) {  // 4x suits per card
        deck.push_back(value);
      }
    }
    for (int count = 0; count < 4 * 3;
         ++count) { // 3x face cards, 4x suits per card
      deck.push_back(10);
    }
    for (int count = 0; count < 4; ++count) { // 4x suits of ace
      deck.push_back(11);
    }
  }
}

void shuffleDeck(std::vector<int> &deck, std::mt19937 &rng, Stats &stats) {
  initDeck(deck);
  std::shuffle(std::begin(deck), std::end(deck), rng);
  stats.shuffles++;
  stats.cardsSinceShuffle = 0;
  stats.runningCount = 0;
  stats.trueCount = 0;
}

void resetHand(Hand &hand, const int64_t &bet) {
  hand.cardCount = 0;
  hand.value = 0;
  hand.aceCount = 0;
  hand.bet = bet;
  hand.doubled = false;
  hand.splitAces = false;
}

void shuffleIfNeeded(std::vector<int> &deck, std::mt19937 &rng, Stats &stats) {
  const int maxCardsBeforeShuffle = config.numberDecks * 52;
  const int penetrationLimit =
      static_cast<int>(config.penetrationBeforeShuffle *
                       static_cast<float>(maxCardsBeforeShuffle));

  if (stats.cardsSinceShuffle > maxCardsBeforeShuffle - 20 ||
      stats.cardsSinceShuffle > penetrationLimit) {
    shuffleDeck(deck, rng, stats);
  }
}

void dealInitialCards(std::vector<int> &deck, Hand &handPlayer, Hand &handDealer,
                      std::mt19937 &rng, const int64_t &bet, Stats &stats) {
  resetHand(handPlayer, bet);
  resetHand(handDealer);

  stats.bank -= bet;
  stats.totalBet += bet;

  shuffleIfNeeded(deck, rng, stats);

  drawCard(deck, handDealer, true, stats);
  drawCard(deck, handPlayer, true, stats);
  drawCard(deck, handDealer, false, stats);
  drawCard(deck, handPlayer, true, stats);
}

Hand makeHand(const int64_t &bet) {
  Hand h;
  resetHand(h, bet);
  return h;
}

Hand split(std::vector<int> &deck, Hand &originalHand, Stats &stats) {
  stats.bank -= originalHand.bet; // bet for newHand
  stats.totalBet += originalHand.bet;
  Hand newHand = makeHand(originalHand.bet);

  const int card = originalHand.cards[originalHand.cardCount - 1];

  originalHand.value = card;
  originalHand.cardCount--;

  if (card == 11) {
    originalHand.splitAces = true;
    newHand.splitAces = true;
    originalHand.value = 11;
    originalHand.aceCount = 1;
    newHand.aceCount = 1;
  }

  newHand.cards[newHand.cardCount++] = card;
  newHand.value = card;

  drawCard(deck, originalHand, true, stats);
  drawCard(deck, newHand, true, stats);

  stats.splits++;
  return newHand;
}

void doubleDown(std::vector<int> &deck, Hand &hand, Stats &stats) {
  stats.bank -= hand.bet; // second bet for double down
  stats.totalBet += hand.bet;
  hand.bet *= 2;
  hand.doubled = true;
  stats.doubles++;
  drawCard(deck, hand, true, stats);
}

void getTrueCount(const std::vector<int> &deck, Stats &stats) {
  const double decksRemaining = static_cast<double>(deck.size()) / 52.0;
  stats.trueCount = static_cast<double>(stats.runningCount) / decksRemaining;
}

int64_t betFromTrueCount(const Stats &stats) {
  if (stats.trueCount <= 0)
    return 1;
  if (stats.trueCount <= 2)
    return 3;
  if (stats.trueCount <= 3)
    return 6;
  if (stats.trueCount <= 4)
    return 10;
  if (stats.trueCount <= 5)
    return 14;
  return 16;
}

bool isBlackjack(const Hand &hand) {
  return !hand.splitAces && hand.cardCount == 2 && hand.value == 21;
}

bool detectBlackjacks(const Hand &handPlayer, const Hand &handDealer,
                      const int64_t &bet, Stats &stats) {
  const bool playerBJ = isBlackjack(handPlayer);
  const bool dealerBJ = isBlackjack(handDealer);
  const int hole = handDealer.cards[1];

  if (playerBJ && dealerBJ) {
    stats.runningCount += countTable[hole];
    stats.draw++;
    announceIfInteractive("Push");
    stats.bank += bet; // return original bet
    return true;
  }
  if (dealerBJ) {
    stats.runningCount += countTable[hole];
    stats.dealerWins++;
    stats.dealerBlackjacks++;
    announceIfInteractive("Dealer Blackjack");
    return true;
  }
  if (playerBJ) {
    stats.playerWins++;
    stats.playerBlackjacks++;
    announceIfInteractive("Player Blackjack");
    stats.bank += static_cast<int64_t>(static_cast<double>(bet) *
                                       2.5); // original bet + 1.5x
    return true;
  }
  return false;
}

void playDealerHand(std::vector<int> &deck, Hand &hand, Stats &stats) {
  while (hand.value < 17 ||
         (config.dealerHitSoft17 && hand.value == 17 && hand.aceCount > 0)) {
    drawCard(deck, hand, true, stats);
  }
}

void resolveHand(const Hand &player, const Hand &dealer, Stats &stats) {
  if (config.isInteractive) {
    printHandState("Player", player);
    printHandState("Dealer", dealer);
  }

  const bool playerBust = player.value > 21;
  const bool dealerBust = dealer.value > 21;
  const bool playerWins =
      !playerBust && (dealerBust || player.value > dealer.value);
  const bool dealerWins =
      !dealerBust && !playerWins && player.value < dealer.value;
  const bool push = !playerBust && !dealerBust && player.value == dealer.value;

  if (playerBust) {
    ++stats.dealerWins;
    announceIfInteractive("Player Bust");
  } else if (dealerBust) {
    ++stats.playerWins;
    stats.bank += player.bet * 2;
    announceIfInteractive("Dealer Bust");
  } else if (playerWins) {
    ++stats.playerWins;
    stats.bank += player.bet * 2;
    announceIfInteractive("Player Win");
  } else if (dealerWins) {
    ++stats.dealerWins;
    announceIfInteractive("Dealer Win");
  } else if (push) {
    ++stats.draw;
    stats.bank += player.bet;
    announceIfInteractive("Push");
  }
}