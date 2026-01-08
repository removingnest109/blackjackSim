#include <array>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>
#include <getopt.h>

int numberHands = 10000000;
int numberDecks = 6;
int startingBank = 100000;
int defaultBetSize = 10;
float penetrationBeforeShuffle = 0.75;
bool dealerHitSoft17 = false;
bool isInteractive = false;
bool cardCounting = false;
bool verbose = false;
bool debtAllowed = false;
bool multiThread = false;

static constexpr int8_t countTable[12] = {0,0,1,1,1,1,1,0,0,0,-1,-1};

struct stats {
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
    int64_t bank = startingBank;
    int64_t totalBet = 0;

    stats& operator+=(const stats& o) {
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
        bank += (o.bank );
        return *this;
    }
};

struct Hand {
    std::array<int, 22> cards{};
    int cardCount = 0;
    int value = 0;
    int aceCount = 0;
    int64_t bet{};
    bool doubled = false;
    bool splitAces = false;

    [[nodiscard]] bool isSoft() const {
        return value <= 21 && aceCount > 0;
    }
};

struct Deck {
    std::vector<int> cards;
    int size = 0;
};

enum class Action {
    Hit,
    Double,
    Split,
    Stand
};

// FAST RNG
struct FastRNG {
    uint64_t state;
    explicit FastRNG(const uint64_t& seed) : state(seed) {
        if (state == 0) state = 0xACE1;
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
// END FAST RNG

// PRINT
void printGlobalVars(const uint32_t& threads) {
    std::cout << "SETTINGS" << std::endl;
    std::cout << "Multithreading: " << (multiThread ? "Enabled" : "Disabled") << std::endl;
    if (multiThread) std::cout << "Number of threads: " << threads << std::endl;
    std::cout << "Number of hands per thread: " << numberHands << std::endl;
    std::cout << "Starting bank: " << startingBank << std::endl;
    std::cout << "Default bet size: " << defaultBetSize << std::endl;
    std::cout << "Number of decks: " << numberDecks << std::endl;
    std::cout << "Penetration before shuffle: " << penetrationBeforeShuffle * 100 << "%" << std::endl;
    std::cout << "Dealer " << (dealerHitSoft17 ? "hits" : "stands") << " on soft 17" << std::endl;
    std::cout << "Card counting: " << (cardCounting ? "Enabled" : "Disabled") << std::endl;
    std::cout << "Negative bank: " << (debtAllowed ? "Enabled" : "Disabled") << std::endl;
    std::cout << std::endl;
}

void printStats(const stats& stats, const uint32_t& threads) {
    const int64_t profit = stats.bank - (startingBank * threads);
    const auto evPercent = static_cast<double>(profit) / static_cast<double>(stats.totalBet);
    if (verbose) {
        const double evPerHand = static_cast<double>(profit) / static_cast<double>(stats.hands);
        const double winPercent = static_cast<double>(stats.playerWins) / (static_cast<double>(stats.playerWins) + static_cast<double>(stats.dealerWins));
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
        std::cout << "Average player win percentage: " << winPercent * 100 << "%" << std::endl;
        std::cout << "Average player bank: " << stats.bank / threads << std::endl;
        std::cout << "Average profit: " << profit / threads << std::endl;
        std::cout << "Average EV per hand: " << evPerHand << " $" << std::endl;
    }
    std::cout << "Average EV percentage: " << evPercent * 100 << "%" << std::endl;
}
// END PRINT

// CARD ACTIONS
void drawCard(Deck& deck, Hand& hand, const bool& visible, stats& stats) {
    const int card = deck.cards[deck.size - 1];

    stats.cardsSinceShuffle++;
    stats.cardsDealt++;

    if (cardCounting) {
        stats.runningCount += visible * countTable[card];
    }

    deck.size--;
    hand.cards[hand.cardCount++] = card;
    hand.value += card;
    if (card == 11) hand.aceCount++;

    while (hand.value > 21 && hand.aceCount > 0) {
        hand.value -= 10;
        hand.aceCount--;
    }
}

void initDeck(Deck& deck) {
    deck.size = 0;
    deck.cards.resize(numberDecks * 52);

    for (int d = 0; d < numberDecks; ++d) { // do once per deck
        for (int value = 2; value <= 10; ++value) { // for each value 2-10
            for (int count = 0; count < 4; ++count) { // 4x suits per card
                deck.cards[deck.size++] = value;
            }
        }
        for (int count = 0; count < 4 * 3; ++count) { // 3x face cards, 4x suits per card
            deck.cards[deck.size++] = 10;
        }
        for (int count = 0; count < 4; ++count) { // 4x suits of ace
            deck.cards[deck.size++] = 11;
        }
    }
}

void shuffleDeck(Deck& deck, FastRNG& rng, stats& stats) {
    initDeck(deck);
    std::shuffle(deck.cards.begin(), deck.cards.begin() + deck.size, rng);
    stats.shuffles++;
    stats.cardsSinceShuffle = 0;
    stats.runningCount = 0;
    stats.trueCount = 0;
}

void dealInitialCards(Deck& deck, Hand& handPlayer, Hand& handDealer, FastRNG& rng, const int64_t& bet, stats& stats) {
    handPlayer.cardCount = 0;
    handPlayer.value = 0;
    handPlayer.aceCount = 0;
    handPlayer.doubled = false;
    handPlayer.bet = bet;
    handPlayer.splitAces = false;
    handDealer.cardCount = 0;
    handDealer.value = 0;
    handDealer.aceCount = 0;
    handDealer.doubled = false;
    handDealer.splitAces = false;

    if (stats.cardsSinceShuffle > (numberDecks * 52) - 20) {
        shuffleDeck(deck, rng, stats);
    }

    if (stats.cardsSinceShuffle > static_cast<int>(penetrationBeforeShuffle * static_cast<float>(numberDecks) * 52)) {
        shuffleDeck(deck, rng, stats);
    }

    drawCard(deck, handDealer, true, stats);
    drawCard(deck, handPlayer, true, stats);
    drawCard(deck, handDealer, false, stats);
    drawCard(deck, handPlayer, true, stats);
}

Hand makeHand(const int64_t& bet) {
    Hand h;
    h.cardCount = 0;
    h.value = 0;
    h.aceCount = 0;
    h.bet = bet;
    h.doubled = false;
    h.splitAces = false;
    return h;
}

Hand split(Deck& deck, Hand& originalHand, stats& stats) {
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

void doubleDown(Deck& deck, Hand& hand, stats& stats) {
    stats.bank -= hand.bet; // second bet for double down
    stats.totalBet += hand.bet;
    hand.bet *= 2;
    hand.doubled = true;
    stats.doubles++;
    drawCard(deck, hand, true, stats);
}
// END CARD ACTIONS

// HELPERS
void getTrueCount(const Deck& deck, stats& stats) {
    const double decksRemaining = static_cast<double>(deck.size) / 52.0;
    stats.trueCount = static_cast<double>(stats.runningCount) / decksRemaining;
}

int64_t betFromTrueCount(const stats& stats) {
    if (stats.trueCount <= 0) return 1;
    if (stats.trueCount <= 2) return 3;
    if (stats.trueCount <= 3) return 6;
    if (stats.trueCount <= 4) return 10;
    if (stats.trueCount <= 5) return 14;
    return 16;
}

bool isBlackjack(const Hand& hand) {
    return !hand.splitAces && hand.cardCount == 2 && hand.value == 21;
}

bool detectBlackjacks(const Deck& deck, const Hand& handPlayer, const Hand& handDealer, const int64_t& bet, stats& stats) {
    const bool playerBJ = isBlackjack(handPlayer);
    const bool dealerBJ = isBlackjack(handDealer);
    const int hole = handDealer.cards[1];

    if (playerBJ && dealerBJ) {
        stats.runningCount += countTable[hole];
        stats.draw++;
        if (isInteractive) std::cout << "Push" << std::endl;
        stats.bank += bet; // return original bet
        return true;
    }
    if (dealerBJ) {
        stats.runningCount += countTable[hole];
        stats.dealerWins++;
        stats.dealerBlackjacks++;
        if (isInteractive) std::cout << "Dealer Blackjack" << std::endl;
        return true;
    }
    if (playerBJ) {
        stats.playerWins++;
        stats.playerBlackjacks++;
        if (isInteractive) std::cout << "Player Blackjack" << std::endl;
        stats.bank += static_cast<int64_t>(static_cast<double>(bet) * 2.5); // original bet + 1.5x
        return true;
    }
    return false;
}
// END HELPERS

// HAND LOOKUP TABLES
constexpr auto H = Action::Hit;
constexpr auto D = Action::Double;
constexpr auto P = Action::Split;
constexpr auto S = Action::Stand;

constexpr Action HARD[22][12] = {
    {S,S,S,S,S,S,S,S,S,S,S,S}, /* 0-4 Unused */
    {S,S,S,S,S,S,S,S,S,S,S,S},
    {S,S,S,S,S,S,S,S,S,S,S,S},
    {S,S,S,S,S,S,S,S,S,S,S,S},
    {S,S,S,S,S,S,S,S,S,S,S,S},

    {S,S,H,H,H,H,H,H,H,H,H,H}, /* 5 */
    {S,S,H,H,H,H,H,H,H,H,H,H}, /* 6 */
    {S,S,H,H,H,H,H,H,H,H,H,H}, /* 7 */
    {S,S,H,H,H,H,H,H,H,H,H,H}, /* 8 */
    {S,S,H,D,D,D,D,H,H,H,H,H}, /* 9 */
    {S,S,D,D,D,D,D,D,D,D,H,H}, /* 10 */
    {S,S,D,D,D,D,D,D,D,D,D,D}, /* 11 */
    {S,S,H,H,S,S,S,H,H,H,H,H}, /* 12 */
    {S,S,S,S,S,S,S,H,H,H,H,H}, /* 13 */
    {S,S,S,S,S,S,S,H,H,H,H,H}, /* 14 */
    {S,S,S,S,S,S,S,H,H,H,H,H}, /* 15 */
    {S,S,S,S,S,S,S,H,H,H,H,H}, /* 16 */
    {S,S,S,S,S,S,S,S,S,S,S,S}, /* 17 */
    {S,S,S,S,S,S,S,S,S,S,S,S}, /* 18 */
    {S,S,S,S,S,S,S,S,S,S,S,S}, /* 19 */
    {S,S,S,S,S,S,S,S,S,S,S,S}, /* 20 */
    {S,S,S,S,S,S,S,S,S,S,S,S}  /* 21 */
};

constexpr Action SOFT[22][12] = {
    {S,S,S,S,S,S,S,S,S,S,S,S}, /* 0–12 Unused */
    {S,S,S,S,S,S,S,S,S,S,S,S},
    {S,S,S,S,S,S,S,S,S,S,S,S},
    {S,S,S,S,S,S,S,S,S,S,S,S},
    {S,S,S,S,S,S,S,S,S,S,S,S},
    {S,S,S,S,S,S,S,S,S,S,S,S},
    {S,S,S,S,S,S,S,S,S,S,S,S},
    {S,S,S,S,S,S,S,S,S,S,S,S},
    {S,S,S,S,S,S,S,S,S,S,S,S},
    {S,S,S,S,S,S,S,S,S,S,S,S},
    {S,S,S,S,S,S,S,S,S,S,S,S},
    {S,S,S,S,S,S,S,S,S,S,S,S},
    {S,S,S,S,S,S,S,S,S,S,S,S},

    {S,S,H,H,H,D,D,H,H,H,H,H}, /* 13 A2 */
    {S,S,H,H,H,D,D,H,H,H,H,H}, /* 14 A3 */
    {S,S,H,H,D,D,D,H,H,H,H,H}, /* 15 A4 */
    {S,S,H,H,D,D,D,H,H,H,H,H}, /* 16 A5 */
    {S,S,H,D,D,D,D,H,H,H,H,H}, /* 17 A6 */
    {S,S,S,D,D,D,D,S,H,H,H,H}, /* 18 A7 */
    {S,S,S,S,S,S,S,S,S,S,S,S}, /* 19 A8 */
    {S,S,S,S,S,S,S,S,S,S,S,S}, /* 20 A9 */
    {S,S,S,S,S,S,S,S,S,S,S,S}  /* 21 */
};

constexpr Action PAIR[12][12] = {
    {S,S,S,S,S,S,S,S,S,S,S,S}, /* 0-1 Unused */
    {S,S,S,S,S,S,S,S,S,S,S,S},

    {S,S,P,P,P,P,P,P,H,H,H,H}, /* 2,2 */
    {S,S,P,P,P,P,P,P,H,H,H,H}, /* 3,3 */
    {S,S,H,H,H,P,P,H,H,H,H,H}, /* 4,4 */
    {S,S,D,D,D,D,D,D,D,D,H,H}, /* 5,5 */
    {S,S,P,P,P,P,P,H,H,H,H,H}, /* 6,6 */
    {S,S,P,P,P,P,P,P,H,H,H,H}, /* 7,7 */
    {S,S,P,P,P,P,P,P,P,P,P,P}, /* 8,8 */
    {S,S,P,P,P,P,P,S,P,P,S,S}, /* 9,9 */
    {S,S,S,S,S,S,S,S,S,S,S,S}, /*10,10*/
    {S,S,P,P,P,P,P,P,P,P,P,P}  /* A,A */
};

Action getAction(const int& total, const int& dealerUp, const bool& isSoft, const bool& isPair, const int& pairRank)
{
    if (isPair) return PAIR[pairRank][dealerUp];
    if (isSoft) return SOFT[total][dealerUp];
    return HARD[total][dealerUp];
}
// END HAND LOOKUP TABLES

// TURN ACTIONS
void playPlayerHands(Deck& deck, Hand hands[], int& handCount, const Hand& dealer, stats& stats) {
    for (int i = 0; i < handCount; ++i) {
        bool done = false;
        while (!done) {
            Hand& hand = hands[i];
            const int dealerUp = dealer.cards[0];

            if (hand.splitAces || hand.value >= 21) {
                break;
            }

            switch (getAction(hand.value, dealerUp, hand.isSoft(), hand.cardCount == 2 && hand.cards[0] == hand.cards[1] && handCount < 4, hand.cards[0])) {
                case Action::Hit:    drawCard(deck, hand, true, stats); break;
                case Action::Double: doubleDown(deck, hand, stats); done = true; break;
                case Action::Split:  hands[handCount++] = split(deck, hand, stats); break;
                case Action::Stand:  default: done = true; break;
            }
        }
    }
}

void interactiveHand(Deck& deck, Hand hands[], int& handCount, const Hand& dealer, stats& stats) {
    for (int i = 0; i < handCount; ++i) {
        while (true) {
            Hand& hand = hands[i];

            std::cout << "Hand " << (i + 1) << " (bet $" << hand.bet << "): ";
            for (int j = 0; j < hand.cardCount; ++j) std::cout << hand.cards[j] << " ";
            std::cout << " -> " << hand.value << std::endl;
            std::cout << "Dealer showing: " << dealer.cards[0] << std::endl;

            if (hand.value >= 21 || hand.splitAces) {
                break;
            }

            std::cout << "[h]it  [s]tand";
            const bool canDouble = hand.cardCount == 2 && !hand.doubled && stats.bank >= hand.bet;
            const bool canSplit = hand.cardCount == 2 && hand.cards[0] == hand.cards[1] && handCount < 4 && stats.bank >= hand.bet;
            if (canDouble) std::cout << "  [d]ouble";
            if (canSplit)  std::cout << "  s[p]lit";
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

void playDealerHand(Deck& deck, Hand& hand, stats& stats) {
    while (hand.value < 17 || (dealerHitSoft17 && hand.value == 17 && hand.aceCount > 0)) {
        drawCard(deck, hand, true, stats);
    }
}

void resolveHand(const Hand& player, const Hand& dealer, stats& stats) {
    if (isInteractive) {
        std::cout << "Player Hand: ";
        for (int i = 0; i < player.cardCount; ++i) std::cout << player.cards[i] << " ";
        std::cout << " -> " << player.value << std::endl;
        std::cout << "Dealer Hand: ";
        for (int i = 0; i < dealer.cardCount; ++i) std::cout << dealer.cards[i] << " ";
        std::cout << " -> " << dealer.value << std::endl;
    }

    if (player.value > 21) {
        stats.dealerWins++;
        if (isInteractive) std::cout << "Player Bust" << std::endl;
    } else if (dealer.value > 21) {
        stats.playerWins++;
        if (isInteractive) std::cout << "Dealer Bust" << std::endl;
        stats.bank += player.bet * 2;
    } else if (player.value > dealer.value) {
        stats.playerWins++;
        if (isInteractive) std::cout << "Player Win" << std::endl;
        stats.bank += player.bet * 2;
    } else if (player.value < dealer.value) {
        stats.dealerWins++;
        if (isInteractive) std::cout << "Dealer Win" << std::endl;
    } else {
        stats.draw++;
        if (isInteractive) std::cout << "Push" << std::endl;
        stats.bank += player.bet;
    }
}

void turnFull(Deck& deck, Hand& dealer, FastRNG& rng, const int64_t& bet, stats& stats) {
    Hand hands[4];
    int handCount = 1;
    stats.bank -= bet; // upfront
    stats.totalBet += bet;
    hands[0] = makeHand(bet);

    dealInitialCards(deck, hands[0], dealer, rng, bet, stats);
    stats.hands++;

    if (detectBlackjacks(deck, hands[0], dealer, bet, stats)) return;

    if (isInteractive) {
        std::cout << "Round " << stats.hands << std::endl;
        interactiveHand(deck, hands, handCount, dealer, stats);
    } else {
        playPlayerHands(deck, hands, handCount, dealer, stats);
    }
    playDealerHand(deck, dealer, stats);

    for (int i = 0; i < handCount; ++i) {
        if (isInteractive) std::cout << "Hand " << (i + 1) << std::endl;
        resolveHand(hands[i], dealer, stats);
    }
}
// END TURN ACTIONS

void runSim(stats& outStats, const uint64_t& seed) {
    stats local;
    FastRNG rng(seed);

    Deck deck;
    shuffleDeck(deck, rng, local);

    Hand dealer;

    for (uint64_t i = 0; i < numberHands; ++i) {
        if (cardCounting) getTrueCount(deck, local);
        int64_t bet = cardCounting ? betFromTrueCount(local) * defaultBetSize : defaultBetSize;
        if (isInteractive) {
            if (cardCounting) std::cout << "count (true count): " << local.runningCount << " (" << std::setprecision (2) << std::fixed << local.trueCount << ")" << std::endl;
            std::cout << "bank: $" << local.bank << std::endl;
            std::cout << "enter bet: $";
            std::cin >> bet;
        }
        if (local.bank < bet && !debtAllowed) break;
        turnFull(deck, dealer, rng, bet, local);
    }

    outStats = local;
}

int main(const int argc, char** argv) {
    const option long_opts[] = {
        {"help", no_argument, nullptr, 'h'},
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
        {nullptr, 0, nullptr, 0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "hvn:d:b:t:p:sicem", long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'h':
                std::cout <<
                    "Options:\n"
                    "  -h, --help                     Show help\n"
                    "  -v, --verbose                  Enable verbose mode\n"
                    "  -n, --hands <num>              Number of hands       (default 10000000)\n"
                    "  -d, --decks <num>              Number of decks       (default 6)\n"
                    "  -b, --bank <amount>            Starting bank         (default 100000)\n"
                    "  -t, --bet <amount>             Default bet size      (default 10)\n"
                    "  -p, --penetration <0.0-1.0>    Shuffle penetration   (default 0.75)\n"
                    "  -s, --dealer-hit-soft-17       Dealer hits soft 17   (default false)\n"
                    "  -i, --interactive              Interactive mode      (default false)\n"
                    "  -c, --card-counting            Enable card counting  (default false)\n"
                    "  -e, --debt                     Enable negative bank  (default false)\n"
                    "  -m, --multithread              Enable multithreading (default false)\n";
                std::exit(0);

            case 'v':
                verbose = true;
                break;

            case 'n':
                numberHands = std::stoi(optarg);
                break;

            case 'd':
                numberDecks = std::stoi(optarg);
                break;

            case 'b':
                startingBank = std::stoi(optarg);
                break;

            case 't':
                defaultBetSize = std::stoi(optarg);
                break;

            case 'p':
                penetrationBeforeShuffle = std::stof(optarg);
                break;

            case 's':
                dealerHitSoft17 = true;
                break;

            case 'i':
                isInteractive = true;
                break;

            case 'c':
                cardCounting = true;
                break;

            case 'e':
                debtAllowed = true;
                break;

            case 'm':
                multiThread = true;
                break;

            default:
                std::exit(1);
        }
    }

    uint32_t threads = std::thread::hardware_concurrency();
    if (isInteractive || !multiThread) threads = 1;

    if (verbose) printGlobalVars(threads);

    std::vector<std::thread> workers;
    std::vector<stats> results(threads);
    std::random_device dev;

    workers.reserve(threads);
    for (unsigned i = 0; i < threads; ++i) {
        workers.emplace_back(
            runSim,
            std::ref(results[i]),
            dev() + i
        );
    }

    for (auto& t : workers) t.join();

    stats global{};
    global.bank = 0;
    for (const auto& s : results) {
        global += s;
    }

    printStats(global, threads);
}
