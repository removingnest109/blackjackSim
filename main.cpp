#include <array>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>

struct Config {
  int numberHands = 10000000;
  int numberDecks = 6;
  int startingBank = 100000;
  int defaultBetSize = 10;
  uint threads = 1;
  float penetrationBeforeShuffle = 0.75;
  bool dealerHitSoft17 = false;
  bool isInteractive = false;
  bool cardCounting = false;
  bool verbose = false;
  bool debtAllowed = false;
  bool multiThread = false;
};

Config config;

struct Stats {
  int64_t hands = 0;
  int64_t playerWins = 0;
  int64_t dealerWins = 0;
  int64_t playerBlackjacks = 0;
  int64_t dealerBlackjacks = 0;
  int64_t draw = 0;
  int64_t shuffles = 0;
  int64_t cardsDealt = 0;
  int64_t splits = 0;
  int64_t doubles = 0;
  int64_t cardsSinceShuffle = 0;
  int64_t runningCount = 0;
  double trueCount = 0;
  int64_t bank = config.startingBank;
  int64_t totalBet = 0;

  Stats &operator+=(const Stats &o) {
    hands += o.hands;
    playerWins += o.playerWins;
    dealerWins += o.dealerWins;
    playerBlackjacks += o.playerBlackjacks;
    dealerBlackjacks += o.dealerBlackjacks;
    draw += o.draw;
    shuffles += o.shuffles;
    cardsDealt += o.cardsDealt;
    splits += o.splits;
    doubles += o.doubles;
    totalBet += o.totalBet;
    bank += (o.bank);
    return *this;
  }
};

static double divide(const int64_t numerator, const int64_t denominator) {
  return denominator == 0 ? 0.0
                          : static_cast<double>(numerator) /
                                static_cast<double>(denominator);
}

void printGlobalVars() {
  std::cout << "SETTINGS" << std::endl;
  std::cout << "Multithreading: "
            << (config.multiThread ? "Enabled" : "Disabled") << std::endl;
  if (config.multiThread)
    std::cout << "Number of threads: " << config.threads << std::endl;
  std::cout << "Number of hands per thread: " << config.numberHands
            << std::endl;
  std::cout << "Starting bank: " << config.startingBank << std::endl;
  std::cout << "Default bet size: " << config.defaultBetSize << std::endl;
  std::cout << "Number of decks: " << config.numberDecks << std::endl;
  std::cout << "Penetration before shuffle: "
            << config.penetrationBeforeShuffle * 100 << "%" << std::endl;
  std::cout << "Dealer " << (config.dealerHitSoft17 ? "hits" : "stands")
            << " on soft 17" << std::endl;
  std::cout << "Card counting: "
            << (config.cardCounting ? "Enabled" : "Disabled") << std::endl;
  std::cout << "Negative bank: "
            << (config.debtAllowed ? "Enabled" : "Disabled") << std::endl;
  std::cout << std::endl;
}

void printStats(const Stats &stats) {
  const int64_t profit = stats.bank - (config.startingBank * config.threads);
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
    std::cout << "Average player bank: " << divide(stats.bank, config.threads)
              << std::endl;
    std::cout << "Average profit: " << divide(profit, config.threads)
              << std::endl;
    std::cout << "Average EV per hand: " << evPerHand << " $" << std::endl;
  }
  std::cout << "Average EV percentage: " << evPercent * 100 << "%" << std::endl;
}

void announceIfInteractive(const std::string &message) {
  if (config.isInteractive)
    std::cout << message << std::endl;
}

struct Hand {
  std::array<int, 22> cards{};
  int cardCount = 0;
  int value = 0;
  int aceCount = 0;
  int64_t bet{};
  bool doubled = false;
  bool splitAces = false;

  [[nodiscard]] bool isSoft() const { return value <= 21 && aceCount > 0; }
};

struct Deck {
  std::vector<int> cards;
  int size = 0;
};

struct FastRNG {
  uint64_t state;
  explicit FastRNG(const uint64_t &seed) : state(seed) {
    if (state == 0)
      state = 0xACE1;
  }
  uint64_t operator()() {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
  }
  static constexpr uint64_t min() { return 0; }
  static constexpr uint64_t max() { return UINT64_MAX; }
  using result_type = uint64_t;
};

static constexpr int8_t countTable[12] = {0, 0, 1, 1, 1, 1, 1, 0, 0, 0, -1, -1};

void drawCard(Deck &deck, Hand &hand, const bool &visible, Stats &stats) {
  const int card = deck.cards[deck.size - 1];

  stats.cardsSinceShuffle++;
  stats.cardsDealt++;

  if (config.cardCounting) {
    stats.runningCount += visible * countTable[card];
  }

  deck.size--;
  hand.cards[hand.cardCount++] = card;
  hand.value += card;
  if (card == 11)
    hand.aceCount++;

  while (hand.value > 21 && hand.aceCount > 0) {
    hand.value -= 10;
    hand.aceCount--;
  }
}

void initDeck(Deck &deck) {
  deck.size = 0;
  deck.cards.resize(config.numberDecks * 52);

  for (int d = 0; d < config.numberDecks; ++d) { // do once per deck
    for (int value = 2; value <= 10; ++value) {  // for each value 2-10
      for (int count = 0; count < 4; ++count) {  // 4x suits per card
        deck.cards[deck.size++] = value;
      }
    }
    for (int count = 0; count < 4 * 3;
         ++count) { // 3x face cards, 4x suits per card
      deck.cards[deck.size++] = 10;
    }
    for (int count = 0; count < 4; ++count) { // 4x suits of ace
      deck.cards[deck.size++] = 11;
    }
  }
}

void shuffleDeck(Deck &deck, FastRNG &rng, Stats &stats) {
  initDeck(deck);
  std::shuffle(deck.cards.begin(), deck.cards.begin() + deck.size, rng);
  stats.shuffles++;
  stats.cardsSinceShuffle = 0;
  stats.runningCount = 0;
  stats.trueCount = 0;
}

void resetHand(Hand &hand, const int64_t &bet = 0) {
  hand.cardCount = 0;
  hand.value = 0;
  hand.aceCount = 0;
  hand.bet = bet;
  hand.doubled = false;
  hand.splitAces = false;
}

void shuffleIfNeeded(Deck &deck, FastRNG &rng, Stats &stats) {
  const int maxCardsBeforeShuffle = config.numberDecks * 52;
  const int penetrationLimit =
      static_cast<int>(config.penetrationBeforeShuffle *
                       static_cast<float>(maxCardsBeforeShuffle));

  if (stats.cardsSinceShuffle > maxCardsBeforeShuffle - 20 ||
      stats.cardsSinceShuffle > penetrationLimit) {
    shuffleDeck(deck, rng, stats);
  }
}

void dealInitialCards(Deck &deck, Hand &handPlayer, Hand &handDealer,
                      FastRNG &rng, const int64_t &bet, Stats &stats) {
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

Hand split(Deck &deck, Hand &originalHand, Stats &stats) {
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

void doubleDown(Deck &deck, Hand &hand, Stats &stats) {
  stats.bank -= hand.bet; // second bet for double down
  stats.totalBet += hand.bet;
  hand.bet *= 2;
  hand.doubled = true;
  stats.doubles++;
  drawCard(deck, hand, true, stats);
}

void getTrueCount(const Deck &deck, Stats &stats) {
  const double decksRemaining = static_cast<double>(deck.size) / 52.0;
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

enum class Action { Hit, Double, Split, Stand };

constexpr auto H = Action::Hit;
constexpr auto D = Action::Double;
constexpr auto P = Action::Split;
constexpr auto S = Action::Stand;

constexpr Action HARD[22][12] = {
    {S, S, S, S, S, S, S, S, S, S, S, S}, /* 0-4 Unused */
    {S, S, S, S, S, S, S, S, S, S, S, S}, {S, S, S, S, S, S, S, S, S, S, S, S},
    {S, S, S, S, S, S, S, S, S, S, S, S}, {S, S, S, S, S, S, S, S, S, S, S, S},

    {S, S, H, H, H, H, H, H, H, H, H, H}, /* 5 */
    {S, S, H, H, H, H, H, H, H, H, H, H}, /* 6 */
    {S, S, H, H, H, H, H, H, H, H, H, H}, /* 7 */
    {S, S, H, H, H, H, H, H, H, H, H, H}, /* 8 */
    {S, S, H, D, D, D, D, H, H, H, H, H}, /* 9 */
    {S, S, D, D, D, D, D, D, D, D, H, H}, /* 10 */
    {S, S, D, D, D, D, D, D, D, D, D, D}, /* 11 */
    {S, S, H, H, S, S, S, H, H, H, H, H}, /* 12 */
    {S, S, S, S, S, S, S, H, H, H, H, H}, /* 13 */
    {S, S, S, S, S, S, S, H, H, H, H, H}, /* 14 */
    {S, S, S, S, S, S, S, H, H, H, H, H}, /* 15 */
    {S, S, S, S, S, S, S, H, H, H, H, H}, /* 16 */
    {S, S, S, S, S, S, S, S, S, S, S, S}, /* 17 */
    {S, S, S, S, S, S, S, S, S, S, S, S}, /* 18 */
    {S, S, S, S, S, S, S, S, S, S, S, S}, /* 19 */
    {S, S, S, S, S, S, S, S, S, S, S, S}, /* 20 */
    {S, S, S, S, S, S, S, S, S, S, S, S}  /* 21 */
};

constexpr Action SOFT[22][12] = {
    {S, S, S, S, S, S, S, S, S, S, S, S}, /* 0–12 Unused */
    {S, S, S, S, S, S, S, S, S, S, S, S}, {S, S, S, S, S, S, S, S, S, S, S, S},
    {S, S, S, S, S, S, S, S, S, S, S, S}, {S, S, S, S, S, S, S, S, S, S, S, S},
    {S, S, S, S, S, S, S, S, S, S, S, S}, {S, S, S, S, S, S, S, S, S, S, S, S},
    {S, S, S, S, S, S, S, S, S, S, S, S}, {S, S, S, S, S, S, S, S, S, S, S, S},
    {S, S, S, S, S, S, S, S, S, S, S, S}, {S, S, S, S, S, S, S, S, S, S, S, S},
    {S, S, S, S, S, S, S, S, S, S, S, S}, {S, S, S, S, S, S, S, S, S, S, S, S},

    {S, S, H, H, H, D, D, H, H, H, H, H}, /* 13 A2 */
    {S, S, H, H, H, D, D, H, H, H, H, H}, /* 14 A3 */
    {S, S, H, H, D, D, D, H, H, H, H, H}, /* 15 A4 */
    {S, S, H, H, D, D, D, H, H, H, H, H}, /* 16 A5 */
    {S, S, H, D, D, D, D, H, H, H, H, H}, /* 17 A6 */
    {S, S, S, D, D, D, D, S, H, H, H, H}, /* 18 A7 */
    {S, S, S, S, S, S, S, S, S, S, S, S}, /* 19 A8 */
    {S, S, S, S, S, S, S, S, S, S, S, S}, /* 20 A9 */
    {S, S, S, S, S, S, S, S, S, S, S, S}  /* 21 */
};

constexpr Action PAIR[12][12] = {
    {S, S, S, S, S, S, S, S, S, S, S, S}, /* 0-1 Unused */
    {S, S, S, S, S, S, S, S, S, S, S, S},

    {S, S, P, P, P, P, P, P, H, H, H, H}, /* 2,2 */
    {S, S, P, P, P, P, P, P, H, H, H, H}, /* 3,3 */
    {S, S, H, H, H, P, P, H, H, H, H, H}, /* 4,4 */
    {S, S, D, D, D, D, D, D, D, D, H, H}, /* 5,5 */
    {S, S, P, P, P, P, P, H, H, H, H, H}, /* 6,6 */
    {S, S, P, P, P, P, P, P, H, H, H, H}, /* 7,7 */
    {S, S, P, P, P, P, P, P, P, P, P, P}, /* 8,8 */
    {S, S, P, P, P, P, P, S, P, P, S, S}, /* 9,9 */
    {S, S, S, S, S, S, S, S, S, S, S, S}, /*10,10*/
    {S, S, P, P, P, P, P, P, P, P, P, P}  /* A,A */
};

Action getAction(const int &total, const int &dealerUp, const bool &isSoft,
                 const bool &isPair, const int &pairRank) {
  if (isPair)
    return PAIR[pairRank][dealerUp];
  if (isSoft)
    return SOFT[total][dealerUp];
  return HARD[total][dealerUp];
}

void simulatePlayerHands(Deck &deck, Hand hands[], int &handCount,
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

void interactiveHand(Deck &deck, Hand hands[], int &handCount,
                     const Hand &dealer, Stats &stats) {
  for (int i = 0; i < handCount; ++i) {
    while (true) {
      Hand &hand = hands[i];

      std::cout << "Hand " << (i + 1) << " (bet $" << hand.bet << "): ";
      for (int j = 0; j < hand.cardCount; ++j)
        std::cout << hand.cards[j] << " ";
      std::cout << " -> " << hand.value << std::endl;
      std::cout << "Dealer showing: " << dealer.cards[0] << std::endl;

      if (hand.value >= 21 || hand.splitAces) {
        break;
      }

      std::cout << "[h]it  [s]tand";
      const bool canDouble =
          hand.cardCount == 2 && !hand.doubled && stats.bank >= hand.bet;
      const bool canSplit = hand.cardCount == 2 &&
                            hand.cards[0] == hand.cards[1] && handCount < 4 &&
                            stats.bank >= hand.bet;
      if (canDouble)
        std::cout << "  [d]ouble";
      if (canSplit)
        std::cout << "  s[p]lit";
      std::cout << std::endl;

      char choice;
      std::cin >> choice;
      if (choice == 'h') {
        drawCard(deck, hand, true, stats);
        continue;
      }
      if (choice == 's') {
        break;
      }
      if (choice == 'd' && canDouble) {
        doubleDown(deck, hand, stats);
        break;
      }
      if (choice == 'p' && canSplit) {
        hands[handCount++] = split(deck, hand, stats);
        continue;
      }
      std::cout << "Invalid choice.\n";
    }
  }
}

void playPlayerHands(Deck &deck, Hand hands[], int &handCount,
                     const Hand &dealer, Stats &stats) {
  if (config.isInteractive) {
    std::cout << "Round " << stats.hands << std::endl;
    interactiveHand(deck, hands, handCount, dealer, stats);
  } else {
    simulatePlayerHands(deck, hands, handCount, dealer, stats);
  }
}

void playDealerHand(Deck &deck, Hand &hand, Stats &stats) {
  while (hand.value < 17 ||
         (config.dealerHitSoft17 && hand.value == 17 && hand.aceCount > 0)) {
    drawCard(deck, hand, true, stats);
  }
}

void printHandState(const std::string &label, const Hand &hand) {
  std::cout << label << " Hand: ";
  for (int i = 0; i < hand.cardCount; ++i)
    std::cout << hand.cards[i] << " ";
  std::cout << " -> " << hand.value << std::endl;
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

void turnFull(Deck &deck, Hand &dealer, FastRNG &rng, const int64_t &bet,
              Stats &stats) {
  Hand hands[4];
  int handCount = 1;
  hands[0] = makeHand(bet);

  dealInitialCards(deck, hands[0], dealer, rng, bet, stats);
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

void playHand(Deck &deck, Hand &dealer, FastRNG &rng, Stats &local) {
  if (config.cardCounting)
    getTrueCount(deck, local);
  int64_t bet = config.cardCounting
                    ? betFromTrueCount(local) * config.defaultBetSize
                    : config.defaultBetSize;
  if (config.isInteractive) {
    if (config.cardCounting)
      std::cout << "count (true count): " << local.runningCount << " ("
                << std::setprecision(2) << std::fixed << local.trueCount << ")"
                << std::endl;
    std::cout << "bank: $" << local.bank << std::endl;
    std::cout << "enter bet: $";
    std::cin >> bet;
  }
  if (local.bank < bet && !config.debtAllowed)
    return;
  ;
  turnFull(deck, dealer, rng, bet, local);
}

Stats runSimThread(const uint64_t &seed) {
  Stats local;
  FastRNG rng(seed);

  Deck deck;
  shuffleDeck(deck, rng, local);

  Hand dealer;

  for (uint64_t i = 0; i < config.numberHands; ++i) {
    playHand(deck, dealer, rng, local);
  }

  printStats(local);

  return local;
}

Stats runSim() {
  if (!config.isInteractive && config.multiThread)
    config.threads = std::thread::hardware_concurrency();
  std::vector<std::thread> workers;
  std::vector<Stats> results(config.threads);
  std::random_device dev;

  workers.reserve(config.threads);
  for (unsigned i = 0; i < config.threads; ++i) {
    workers.emplace_back([&, i] { results[i] = runSimThread(dev() + i); });
  }

  for (auto &t : workers)
    t.join();

  Stats global{};
  global.bank = 0;
  for (const auto &s : results) {
    global += s;
  }

  return global;
}

void getArgs(const int argc, char **argv) {
  const option long_opts[] = {{"help", no_argument, nullptr, 'h'},
                              {"verbose", no_argument, nullptr, 'v'},
                              {"hands", required_argument, nullptr, 'n'},
                              {"decks", required_argument, nullptr, 'd'},
                              {"bank", required_argument, nullptr, 'b'},
                              {"bet", required_argument, nullptr, 't'},
                              {"penetration", required_argument, nullptr, 'p'},
                              {"dealer-hit-soft-17", no_argument, nullptr, 's'},
                              {"interactive", no_argument, nullptr, 'i'},
                              {"card-counting", no_argument, nullptr, 'c'},
                              {"debt", no_argument, nullptr, 'e'},
                              {"multithread", no_argument, nullptr, 'm'},
                              {nullptr, 0, nullptr, 0}};

  int opt;
  while ((opt = getopt_long(argc, argv, "hvn:d:b:t:p:sicem", long_opts,
                            nullptr)) != -1) {
    switch (opt) {
    case 'h':
      std::cout << "Options:\n"
                   "  -h, --help                     Show help\n"
                   "  -v, --verbose                  Enable verbose mode\n"
                   "  -n, --hands <num>              Number of hands       "
                   "(default 10000000)\n"
                   "  -d, --decks <num>              Number of decks       "
                   "(default 6)\n"
                   "  -b, --bank <amount>            Starting bank         "
                   "(default 100000)\n"
                   "  -t, --bet <amount>             Default bet size      "
                   "(default 10)\n"
                   "  -p, --penetration <0.0-1.0>    Shuffle penetration   "
                   "(default 0.75)\n"
                   "  -s, --dealer-hit-soft-17       Dealer hits soft 17   "
                   "(default false)\n"
                   "  -i, --interactive              Interactive mode      "
                   "(default false)\n"
                   "  -c, --card-counting            Enable card counting  "
                   "(default false)\n"
                   "  -e, --debt                     Enable negative bank  "
                   "(default false)\n"
                   "  -m, --multithread              Enable multithreading "
                   "(default false)\n";
      std::exit(0);

    case 'v':
      config.verbose = true;
      break;

    case 'n':
      config.numberHands = std::stoi(optarg);
      break;

    case 'd':
      config.numberDecks = std::stoi(optarg);
      break;

    case 'b':
      config.startingBank = std::stoi(optarg);
      break;

    case 't':
      config.defaultBetSize = std::stoi(optarg);
      break;

    case 'p':
      config.penetrationBeforeShuffle = std::stof(optarg);
      break;

    case 's':
      config.dealerHitSoft17 = true;
      break;

    case 'i':
      config.isInteractive = true;
      break;

    case 'c':
      config.cardCounting = true;
      break;

    case 'e':
      config.debtAllowed = true;
      break;

    case 'm':
      config.multiThread = true;
      break;

    default:
      std::exit(1);
    }
  }
}

int main(const int argc, char **argv) {
  getArgs(argc, argv);
  if (config.verbose)
    printGlobalVars();
  printStats(runSim());
  return 0;
}
