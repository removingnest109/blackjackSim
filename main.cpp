#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>
#include <thread>

#define NUMBER_HANDS 10000000
#define NUMBER_DECKS 6
#define PLAYER_STARTING_BANK 100000
#define DEFAULT_BET 10
#define PENETRATION 0.75
#define DEALER_HIT_ON_SOFT_17 false
#define INTERACTIVE false
#define CARD_COUNTING true

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
    int64_t bank = PLAYER_STARTING_BANK;
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
    std::vector<int> cards;
    int64_t bet;
    bool doubled = false;
    bool splitAces = false;
};

enum class Action {
    Hit,
    Double,
    Split,
    Stand
};

// PRINT
void printGlobalVars(const uint threads) {
    std::cout << "Number of independent players simulated: " << threads << std::endl;
    std::cout << "Number of hands per player: " << NUMBER_HANDS << std::endl;
    std::cout << "Player starting bank: " << PLAYER_STARTING_BANK << std::endl;
    std::cout << "Number of decks: " << NUMBER_DECKS << std::endl;
    std::cout << "Penetration before shuffle: " << PENETRATION * 100 << "%" << std::endl;
    std::cout << "Reshuffle at card " << static_cast<int>(PENETRATION * NUMBER_DECKS * 52) << std::endl;
    if constexpr (DEALER_HIT_ON_SOFT_17) {
        std::cout << "Dealer hits on soft 17" << std::endl;
    } else {
        std::cout << "Dealer stands on soft 17" << std::endl;
    }
}

void printStats(const stats& stats, const uint threads) {
    const double winPercent = (static_cast<double>(stats.playerWins) / (static_cast<double>(stats.playerWins) + static_cast<double>(stats.dealerWins)));
    const int64_t profit = stats.bank - (PLAYER_STARTING_BANK * threads);
    const double evPerHand = static_cast<double>(profit) / static_cast<double>(stats.hands);
    const auto evPercent = static_cast<double>(static_cast<double>(profit) / static_cast<double>(stats.totalBet));
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
    std::cout << "Average player win percentage excl draws: " << winPercent * 100 << "%" << std::endl;
    std::cout << "Average player bank: " << stats.bank / threads << std::endl;
    std::cout << "Average profit: " << profit / threads << std::endl;
    std::cout << "Average EV per hand: " << evPerHand << " $" << std::endl;
    std::cout << "Average EV percentage: " << evPercent * 100 << "%" << std::endl;
}
// END PRINT

// CARD ACTIONS
void drawCard(std::vector<int>& deck, std::vector<int>& cards, const bool visible, stats& stats) {
    const int card = deck.back();

    stats.cardsSinceShuffle++;
    stats.cardsDealt++;

    if constexpr (CARD_COUNTING) {
        if (visible) {
            if (card < 7) stats.runningCount++;
            if (card > 9) stats.runningCount--;
        }
    }

    deck.pop_back();
    cards.push_back(card);
}

void initDeck(std::vector<int>& deck) {
    deck.clear();

    for (int d = 0; d < NUMBER_DECKS; ++d) { // do once per deck
        for (int value = 2; value <= 10; ++value) { // for each value 2-10
            for (int count = 0; count < 4; ++count) { // 4x suits per card
                deck.push_back(value);
            }
        }
        for (int count = 0; count < 4 * 3; ++count) { // 3x face cards, 4x suits per card
            deck.push_back(10);
        }
        for (int count = 0; count < 4; ++count) { // 4x suits of ace
            deck.push_back(11);
        }
    }
}

void shuffleDeck(std::vector<int>& deck, std::mt19937& rng, stats& stats) {
    initDeck(deck);
    std::ranges::shuffle(deck, rng);
    stats.shuffles++;
    stats.cardsSinceShuffle = 0;
    stats.runningCount = 0;
    stats.trueCount = 0;
}

void dealInitialCards(std::vector<int>& deck, Hand& handPlayer, std::vector<int>& handDealer, std::mt19937& rng, const int64_t bet, stats& stats) {
    handPlayer.cards.clear();
    handPlayer.doubled = false;
    handPlayer.bet = bet;
    handDealer.clear();

    if (stats.cardsSinceShuffle > (NUMBER_DECKS * 52) - 20) {
        shuffleDeck(deck, rng, stats);
    }

    if (stats.cardsSinceShuffle > static_cast<int>(PENETRATION * NUMBER_DECKS * 52)) {
        shuffleDeck(deck, rng, stats);
    }

    drawCard(deck, handDealer, true, stats);
    drawCard(deck, handPlayer.cards, true, stats);
    drawCard(deck, handDealer, false, stats);
    drawCard(deck, handPlayer.cards, true, stats);
}

std::vector<int> initHand() {
    std::vector<int> cards;
    cards.reserve(22); // most possible number of cards in a hand
    return cards;
}

Hand makeHand(const int64_t bet) {
    return { initHand(), bet, false };
}

Hand split(std::vector<int>& deck, Hand& originalHand, stats& stats) {
    stats.bank -= originalHand.bet; // bet for newHand
    stats.totalBet += originalHand.bet;
    Hand newHand = makeHand(originalHand.bet);

    const int card = originalHand.cards.back();

    if (card == 11) {
        originalHand.splitAces = true;
        newHand.splitAces = true;
    }

    originalHand.cards.pop_back();
    newHand.cards.push_back(card);

    drawCard(deck, originalHand.cards, true, stats);
    drawCard(deck, newHand.cards, true, stats);

    stats.splits++;
    return newHand;
}

void doubleDown(std::vector<int>& deck, Hand& hand, stats& stats) {
    stats.bank -= hand.bet; // second bet for double down
    stats.totalBet += hand.bet;
    hand.bet *= 2;
    hand.doubled = true;
    stats.doubles++;
    drawCard(deck, hand.cards, true, stats);
}
// END CARD ACTIONS

// HELPERS
void getTrueCount(const std::vector<int>& deck, stats& stats) {
    const double decksRemaining = static_cast<double>(deck.size()) / 52.0;
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

int calculateHandValue(const std::vector<int>& cards) {
    int total = 0;
    int aces = 0;

    for (const int card : cards) {
        total += card;
        if (card == 11) aces++;
    }

    while (total > 21 && aces > 0) {
        total -= 10;
        aces--;
    }

    return total;
}

bool isSoftHand(const std::vector<int>& cards) {
    int total = 0;
    int aces = 0;
    for (const int card : cards) {
        total += card;
        if (card == 11) aces++;
    }
    return (total <= 21 && aces > 0);
}

bool isBlackjack(const std::vector<int>& cards, const bool splitAces) {
    return !splitAces && cards.size() == 2 && calculateHandValue(cards) == 21;
}

bool detectBlackjacks(const std::vector<int>& deck, const Hand& handPlayer, const std::vector<int>& handDealer, const int64_t bet, stats& stats) {
    if (isBlackjack(handDealer, false) && isBlackjack(handPlayer.cards, handPlayer.splitAces)) {
        if (const int hole = handDealer[1]; hole < 7) stats.runningCount++;
        else if (hole > 9) stats.runningCount--;
        const double decksRemaining = static_cast<double>(deck.size()) / 52.0;
        stats.trueCount = static_cast<int>(std::floor(static_cast<long double>(stats.runningCount) / decksRemaining));
        stats.draw++;
        if constexpr (INTERACTIVE) std::cout << "Push" << std::endl;
        stats.bank += bet; // return original bet
    } else if (isBlackjack(handDealer, false)) {
        if (const int hole = handDealer[1]; hole < 7) stats.runningCount++;
        else if (hole > 9) stats.runningCount--;
        const double decksRemaining = static_cast<double>(deck.size()) / 52.0;
        stats.trueCount = static_cast<int>(std::floor(static_cast<long double>(stats.runningCount) / decksRemaining));
        stats.dealerWins++;
        stats.dealerBlackjacks++;
        if constexpr (INTERACTIVE) std::cout << "Dealer Blackjack" << std::endl;
    } else if (isBlackjack(handPlayer.cards, handPlayer.splitAces)) {
        stats.playerWins++;
        stats.playerBlackjacks++;
        if constexpr (INTERACTIVE) std::cout << "Player Blackjack" << std::endl;
        stats.bank += static_cast<int64_t>(static_cast<double>(bet) * 2.5); // original bet + 1.5x
    }
    return (isBlackjack(handPlayer.cards, handPlayer.splitAces) || isBlackjack(handDealer, false));
}
// END HELPERS

// HAND LOOKUP TABLES
constexpr auto H = Action::Hit;
constexpr auto D = Action::Double;
constexpr auto P = Action::Split;
constexpr auto S = Action::Stand;

constexpr Action HARD[22][12] = {
    {S,S,S,S,S,S,S,S,S,S,S,S}, /* 0 */
    {S,S,S,S,S,S,S,S,S,S,S,S}, /* 1 */
    {S,S,S,S,S,S,S,S,S,S,S,S}, /* 2 */
    {S,S,S,S,S,S,S,S,S,S,S,S}, /* 3 */
    {S,S,S,S,S,S,S,S,S,S,S,S}, /* 4 */
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
    {S,S,S,S,S,S,S,S,S,S,S,S}, /* 0 */
    {S,S,S,S,S,S,S,S,S,S,S,S}, /* 1 */
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

Action getAction(const int total, const int dealerUp, const bool isSoft, const bool isPair, const int pairRank)
{
    if (isPair && pairRank >= 2 && pairRank <= 11) return PAIR[pairRank][dealerUp];
    if (isSoft && total >= 13 && total <= 20) return SOFT[total][dealerUp];
    if (total >= 5 && total <= 21) return HARD[total][dealerUp];
    return Action::Stand;
}
// END HAND LOOKUP TABLES

// TURN ACTIONS
void playPlayerHands(std::vector<int>& deck, std::vector<Hand>& hands, const std::vector<int>& dealer, stats& stats) {
    for (size_t i = 0; i < hands.size(); ++i) {
        bool done = false;
        while (!done) {
            Hand& hand = hands[i];
            const int dealerUp = dealer[0];
            const int value = calculateHandValue(hand.cards);

            if (hand.splitAces || value >= 21) {
                break;
            }

            switch (getAction(value, dealerUp, isSoftHand(hand.cards), hand.cards.size() == 2 && hand.cards[0] == hand.cards[1] && hands.size() < 4, hand.cards[0])) {
                case Action::Hit:    drawCard(deck, hand.cards, true, stats); break;
                case Action::Double: doubleDown(deck, hand, stats); done = true; break;
                case Action::Split:  hands.push_back(split(deck, hand, stats)); break;
                case Action::Stand:  default: done = true; break;
            }
        }
    }
}

void interactiveHand(std::vector<int>& deck, std::vector<Hand>& hands, const std::vector<int>& dealer, stats& stats) {
    for (size_t i = 0; i < hands.size(); ++i) {
        while (true) {
            Hand& hand = hands[i];
            const int value = calculateHandValue(hand.cards);

            std::cout << "Hand " << (i + 1) << " (bet $" << hand.bet << "): ";
            for (const int c : hand.cards) std::cout << c << " ";
            std::cout << " -> " << value << std::endl;
            std::cout << "Dealer showing: " << dealer[0] << std::endl;

            if (value >= 21 || hand.splitAces) {
                break;
            }

            std::cout << "[h]it  [s]tand";
            const bool canDouble = hand.cards.size() == 2 && !hand.doubled && stats.bank >= hand.bet;
            const bool canSplit = hand.cards.size() == 2 && hand.cards[0] == hand.cards[1] && hands.size() < 4 && stats.bank >= hand.bet;
            if (canDouble) std::cout << "  [d]ouble";
            if (canSplit)  std::cout << "  s[p]lit";
            std::cout << std::endl;

            char choice;
            std::cin >> choice;
            if (choice == 'h') {
                drawCard(deck, hand.cards, true, stats);
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
                hands.push_back(split(deck, hand, stats));
                continue;
            }
            std::cout << "Invalid choice.\n";
        }
    }
}

void playDealerHand(std::vector<int>& deck, std::vector<int>& hand, stats& stats) {
    while (true) {
        const int value = calculateHandValue(hand);
        if (value < 17) {
            drawCard(deck, hand, true, stats);
            continue;
        }
        if (value == 17 && isSoftHand(hand) && DEALER_HIT_ON_SOFT_17) {
            drawCard(deck, hand, true, stats);
            continue;
        }
        break;
    }
}

void resolveHand(const Hand& player, const std::vector<int>& dealer, stats& stats) {
    const int p = calculateHandValue(player.cards);
    const int d = calculateHandValue(dealer);

    if constexpr (INTERACTIVE) {
        std::cout << "Player Hand: ";
        for (const int c : player.cards) std::cout << c << " ";
        std::cout << " -> " << p << std::endl;
        std::cout << "Dealer Hand: ";
        for (const int c : dealer) std::cout << c << " ";
        std::cout << " -> " << d << std::endl;
    }

    if (p > 21) {
        stats.dealerWins++;
        if constexpr (INTERACTIVE) std::cout << "Player Bust" << std::endl;
    } else if (d > 21) {
        stats.playerWins++;
        if constexpr (INTERACTIVE) std::cout << "Dealer Bust" << std::endl;
        stats.bank += player.bet * 2;
    } else if (p > d) {
        stats.playerWins++;
        if constexpr (INTERACTIVE) std::cout << "Player Win" << std::endl;
        stats.bank += player.bet * 2;
    } else if (p < d) {
        stats.dealerWins++;
        if constexpr (INTERACTIVE) std::cout << "Dealer Win" << std::endl;
    } else {
        stats.draw++;
        if constexpr (INTERACTIVE) std::cout << "Push" << std::endl;
        stats.bank += player.bet;
    }
}

void turnFull(std::vector<int>& deck, std::vector<int>& dealer, std::mt19937& rng, const int64_t bet, stats& stats) {
    std::vector<Hand> hands;
    stats.bank -= bet; // upfront
    stats.totalBet += bet;
    hands.push_back(makeHand(bet));

    dealInitialCards(deck, hands[0], dealer, rng, bet, stats);
    stats.hands++;

    if (detectBlackjacks(deck, hands[0], dealer, bet, stats)) return;

    if constexpr (INTERACTIVE) {
        std::cout << "Round " << stats.hands << std::endl;
        interactiveHand(deck,hands,dealer, stats);
    } else {
        playPlayerHands(deck, hands, dealer, stats);
    }
    playDealerHand(deck, dealer, stats);

    for (size_t i = 0; i < hands.size(); ++i) {
        if constexpr (INTERACTIVE) std::cout << "Hand " << (i + 1) << std::endl;
        resolveHand(hands[i], dealer, stats);
    }
}
// END TURN ACTIONS

void runSim(const uint64_t handsToPlay, stats& outStats, const uint64_t seed) {
    stats local;
    std::mt19937 rng(seed);

    std::vector<int> deck;
    deck.reserve(52 * NUMBER_DECKS);
    shuffleDeck(deck, rng, local);

    std::vector<int> dealer = initHand();

    for (uint64_t i = 0; i < handsToPlay; ++i) {
        if constexpr (CARD_COUNTING) getTrueCount(deck, local);
        int64_t bet = CARD_COUNTING ? betFromTrueCount(local) * DEFAULT_BET : DEFAULT_BET;
        if constexpr (INTERACTIVE) {
            if constexpr (CARD_COUNTING) std::cout << "count (true count): " << local.runningCount << " (" << std::setprecision (2) << std::fixed << local.trueCount << ")" << std::endl;
            std::cout << "bank: $" << local.bank << std::endl;
            std::cout << "enter bet: $";
            std::cin >> bet;
        }
        if (local.bank < bet) break;
        turnFull(deck, dealer, rng, bet, local);
    }

    outStats = local;
}

int main() {
    unsigned threads;
    if constexpr (!INTERACTIVE) {
        threads = std::thread::hardware_concurrency();
    } else threads = 1;

    printGlobalVars(threads);

    std::vector<std::thread> workers;
    std::vector<stats> results(threads);
    std::random_device dev;

    for (unsigned i = 0; i < threads; ++i) {
        workers.emplace_back(
            runSim,
            NUMBER_HANDS,
            std::ref(results[i]),
            dev() + i
        );
    }

    for (auto& t : workers) t.join();

    stats global;
    for (const auto& s : results) {
        global += s;
    }

    printStats(global, threads);
}
