#include <algorithm>
#include <iostream>
#include <random>
#include <vector>

constexpr int NUMBER_HANDS = 100000000;
constexpr int NUMBER_DECKS = 6;
constexpr float PENETRATION = 0.75;
constexpr float PLAYER_STARTING_BANK = 1000000;
constexpr float DEFAULT_BET = 100.0;
constexpr bool DEALER_HIT_ON_SOFT_17 = false;
constexpr bool INTERACTIVE = false;
constexpr bool CARD_COUNTING = true;

struct stats {
    int hands = 0;
    int playerWins = 0;
    int dealerWins = 0;
    int playerBlackjacks = 0;
    int dealerBlackjacks = 0;
    int draw = 0;
    int shuffles = 0;
    int cardsDealt = 0;
    int splits = 0;
    int doubles = 0;
    int cardsSinceShuffle = 0;
    int runningCount = 0;
    int trueCount = 0;
    float bank = PLAYER_STARTING_BANK;
    float totalBet = 0;
};

struct Hand {
    std::vector<int> cards;
    float bet;
    bool doubled = false;
    bool splitAces = false;
};

enum class Action {
    Hit,
    Double,
    Split,
    Stand
};

stats stats;

// PRINT
void printGlobalVars() {
    std::cout << "Number of decks: " << NUMBER_DECKS << std::endl;
    std::cout << "Number of hands being played: " << NUMBER_HANDS << std::endl;
    std::cout << "Penetration before shuffle: " << PENETRATION * 100 << "%" << std::endl;
    std::cout << "Reshuffle at card " << static_cast<int>(PENETRATION * NUMBER_DECKS * 52) << std::endl;
    std::cout << "Player starting bank: " << static_cast<int>(PLAYER_STARTING_BANK) << std::endl;
    if constexpr (DEALER_HIT_ON_SOFT_17) {
        std::cout << "Dealer hits on soft 17" << std::endl;
    } else {
        std::cout << "Dealer stands on soft 17" << std::endl;
    }
}

void printStats() {
    const float winPercent = (static_cast<float>(stats.playerWins) / (static_cast<float>(stats.playerWins) + static_cast<float>(stats.dealerWins)));
    const float profit = stats.bank - PLAYER_STARTING_BANK;
    const float evPerHand = profit / static_cast<float>(stats.hands);
    const float evPercent = profit / stats.totalBet;
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
    std::cout << "player win percentage excl draws: " << winPercent * 100 << "%" << std::endl;
    std::cout << "player bank: " << static_cast<int>(stats.bank) << std::endl;
    std::cout << "profit: " << static_cast<int>(profit) << std::endl;
    std::cout << "EV per hand: " << evPerHand << " $" << std::endl;
    std::cout << "EV percentage: " << evPercent * 100 << "%" << std::endl;
}

void printIfInteractive(const std::string& msg) {
    if constexpr (INTERACTIVE) {
        std::cout << msg << std::endl;
    }
}
// END PRINT

// CARD ACTIONS
void drawCard(std::vector<int>& deck, std::vector<int>& cards, const bool visible) {
    const int card = deck.back();

    stats.cardsSinceShuffle++;
    stats.cardsDealt++;

    if (visible) {
        if (card < 7) stats.runningCount++;
        if (card > 9) stats.runningCount--;
        const double decksRemaining = static_cast<double>(deck.size()) / 52.0;
        const double trueCount = stats.runningCount / decksRemaining;
        stats.trueCount = static_cast<int>(std::floor(trueCount));
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

void shuffleDeck(std::vector<int>& deck, std::mt19937& rng) {
    initDeck(deck);
    std::ranges::shuffle(deck, rng);
    stats.shuffles++;
    stats.cardsSinceShuffle = 0;
    stats.runningCount = 0;
    stats.trueCount = 0;
}

void dealInitialCards(std::vector<int>& deck, Hand& handPlayer, std::vector<int>& handDealer, std::mt19937& rng, const float bet) {
    handPlayer.cards.clear();
    handPlayer.doubled = false;
    handPlayer.bet = bet;
    handDealer.clear();

    if (stats.cardsSinceShuffle > (NUMBER_DECKS * 52) - 20) {
        shuffleDeck(deck, rng);
    }

    if (stats.cardsSinceShuffle > static_cast<int>(PENETRATION * NUMBER_DECKS * 52)) {
        shuffleDeck(deck, rng);
    }

    drawCard(deck, handDealer, true);
    drawCard(deck, handPlayer.cards, true);
    drawCard(deck, handDealer, false);
    drawCard(deck, handPlayer.cards, true);
}

std::vector<int> initHand() {
    std::vector<int> cards;
    cards.reserve(22); // most possible number of cards in a hand
    return cards;
}

Hand makeHand(const float bet) {
    return { initHand(), bet, false };
}

Hand split(std::vector<int>& deck, Hand& originalHand) {
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

    drawCard(deck, originalHand.cards, true);
    drawCard(deck, newHand.cards, true);

    stats.splits++;
    return newHand;
}

void doubleDown(std::vector<int>& deck, Hand& hand) {
    stats.bank -= hand.bet; // second bet for double down
    stats.totalBet += hand.bet;
    hand.bet *= 2;
    hand.doubled = true;
    stats.doubles++;
    drawCard(deck, hand.cards, true);
}
// END CARD ACTIONS

// HELPERS
float betFromTrueCount() {
    if (stats.trueCount <= 0) return 1.0f;
    if (stats.trueCount == 1) return 3.0f;
    if (stats.trueCount == 2) return 6.0f;
    if (stats.trueCount == 3) return 10.0f;
    if (stats.trueCount == 4) return 14.0f;
    return 16.0f;
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

bool detectBlackjacks(const std::vector<int>& deck, const Hand& handPlayer, const std::vector<int>& handDealer, const float bet) {
    if (isBlackjack(handDealer, false) && isBlackjack(handPlayer.cards, handPlayer.splitAces)) {
        if (const int hole = handDealer[1]; hole < 7) stats.runningCount++;
        else if (hole > 9) stats.runningCount--;
        const double decksRemaining = static_cast<double>(deck.size()) / 52.0;
        stats.trueCount = static_cast<int>(std::floor(stats.runningCount / decksRemaining));
        stats.draw++;
        printIfInteractive("Push");
        stats.bank += bet; // return original bet
    } else if (isBlackjack(handDealer, false)) {
        if (const int hole = handDealer[1]; hole < 7) stats.runningCount++;
        else if (hole > 9) stats.runningCount--;
        const double decksRemaining = static_cast<double>(deck.size()) / 52.0;
        stats.trueCount = static_cast<int>(std::floor(stats.runningCount / decksRemaining));
        stats.dealerWins++;
        stats.dealerBlackjacks++;
        printIfInteractive("Dealer Blackjack");
    } else if (isBlackjack(handPlayer.cards, handPlayer.splitAces)) {
        stats.playerWins++;
        stats.playerBlackjacks++;
        printIfInteractive("Player Blackjack");
        stats.bank += bet * 2.5f; // original bet + 1.5x
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
    /* 0 */  {S,S,S,S,S,S,S,S,S,S,S,S},
    /* 1 */  {S,S,S,S,S,S,S,S,S,S,S,S},
    /* 2 */  {S,S,S,S,S,S,S,S,S,S,S,S},
    /* 3 */  {S,S,S,S,S,S,S,S,S,S,S,S},
    /* 4 */  {S,S,S,S,S,S,S,S,S,S,S,S},
    /* 5 */  {S,S,H,H,H,H,H,H,H,H,H,H},
    /* 6 */  {S,S,H,H,H,H,H,H,H,H,H,H},
    /* 7 */  {S,S,H,H,H,H,H,H,H,H,H,H},
    /* 8 */  {S,S,H,H,H,H,H,H,H,H,H,H},
    /* 9 */  {S,S,H,D,D,D,D,H,H,H,H,H},
    /* 10 */ {S,S,D,D,D,D,D,D,D,D,H,H},
    /* 11 */ {S,S,D,D,D,D,D,D,D,D,D,D},
    /* 12 */ {S,S,H,H,S,S,S,H,H,H,H,H},
    /* 13 */ {S,S,S,S,S,S,S,H,H,H,H,H},
    /* 14 */ {S,S,S,S,S,S,S,H,H,H,H,H},
    /* 15 */ {S,S,S,S,S,S,S,H,H,H,H,H},
    /* 16 */ {S,S,S,S,S,S,S,H,H,H,H,H},
    /* 17 */ {S,S,S,S,S,S,S,S,S,S,S,S},
    /* 18 */ {S,S,S,S,S,S,S,S,S,S,S,S},
    /* 19 */ {S,S,S,S,S,S,S,S,S,S,S,S},
    /* 20 */ {S,S,S,S,S,S,S,S,S,S,S,S},
    /* 21 */ {S,S,S,S,S,S,S,S,S,S,S,S}
};

constexpr Action SOFT[22][12] = {
    /* 0–12 */ {S,S,S,S,S,S,S,S,S,S,S,S},
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

    /* 13 A2 */ {S,S,H,H,H,D,D,H,H,H,H,H},
    /* 14 A3 */ {S,S,H,H,H,D,D,H,H,H,H,H},
    /* 15 A4 */ {S,S,H,H,D,D,D,H,H,H,H,H},
    /* 16 A5 */ {S,S,H,H,D,D,D,H,H,H,H,H},
    /* 17 A6 */ {S,S,H,D,D,D,D,H,H,H,H,H},
    /* 18 A7 */ {S,S,S,D,D,D,D,S,H,H,H,H},
    /* 19 A8 */ {S,S,S,S,S,S,S,S,S,S,S,S},
    /* 20 A9 */ {S,S,S,S,S,S,S,S,S,S,S,S},
    /* 21 */    {S,S,S,S,S,S,S,S,S,S,S,S}
};

constexpr Action PAIR[12][12] = {
    /* 0 */  {S,S,S,S,S,S,S,S,S,S,S,S},
    /* 1 */  {S,S,S,S,S,S,S,S,S,S,S,S},

    /* 2,2 */ {S,S,P,P,P,P,P,P,H,H,H,H},
    /* 3,3 */ {S,S,P,P,P,P,P,P,H,H,H,H},
    /* 4,4 */ {S,S,H,H,H,P,P,H,H,H,H,H},
    /* 5,5 */ {S,S,D,D,D,D,D,D,D,D,H,H},
    /* 6,6 */ {S,S,P,P,P,P,P,H,H,H,H,H},
    /* 7,7 */ {S,S,P,P,P,P,P,P,H,H,H,H},
    /* 8,8 */ {S,S,P,P,P,P,P,P,P,P,P,P},
    /* 9,9 */ {S,S,P,P,P,P,P,S,P,P,S,S},
    /*10,10*/ {S,S,S,S,S,S,S,S,S,S,S,S},
    /* A,A */  {S,S,P,P,P,P,P,P,P,P,P,P}
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
void playPlayerHands(
    std::vector<int>& deck, std::vector<Hand>& hands, const std::vector<int>& dealer) {
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
                case Action::Hit:    drawCard(deck, hand.cards, true); break;
                case Action::Double: doubleDown(deck, hand); done = true; break;
                case Action::Split:  hands.push_back(split(deck, hand)); break;
                case Action::Stand:  default: done = true; break;
            }
        }
    }
}

void interactiveHand(std::vector<int>& deck, std::vector<Hand>& hands, const std::vector<int>& dealer) {
    for (size_t i = 0; i < hands.size(); ++i) {
        while (true) {
            Hand& hand = hands[i];
            const int value = calculateHandValue(hand.cards);

            std::cout << "Bank: " << static_cast<int>(stats.bank) << std::endl;
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
                drawCard(deck, hand.cards, true);
                continue;
            }
            if (choice == 's') {
                break;
            }
            if (choice == 'd' && canDouble) {
                doubleDown(deck, hand);
                break;
            }
            if (choice == 'p' && canSplit) {
                hands.push_back(split(deck, hand));
                continue;
            }
            std::cout << "Invalid choice.\n";
        }
    }
}

void playDealerHand(std::vector<int>& deck, std::vector<int>& hand) {
    while (true) {
        const int value = calculateHandValue(hand);
        if (value < 17) {
            drawCard(deck, hand, true);
            continue;
        }
        if (value == 17 && isSoftHand(hand) && DEALER_HIT_ON_SOFT_17) {
            drawCard(deck, hand, true);
            continue;
        }
        break;
    }
}

void resolveHand(const Hand& player, const std::vector<int>& dealer) {
    const int p = calculateHandValue(player.cards);
    const int d = calculateHandValue(dealer);

    if (INTERACTIVE) {
        std::cout << "Player Hand: ";
        for (const int c : player.cards) std::cout << c << " ";
        std::cout << " -> " << p << std::endl;
        std::cout << "Dealer Hand: ";
        for (const int c : dealer) std::cout << c << " ";
        std::cout << " -> " << d << std::endl;
    }

    if (p > 21) {
        stats.dealerWins++;
        printIfInteractive("Player Bust");
    } else if (d > 21) {
        stats.playerWins++;
        printIfInteractive("Dealer Bust");
        stats.bank += player.bet * 2;
    } else if (p > d) {
        stats.playerWins++;
        printIfInteractive("Player Win");
        stats.bank += player.bet * 2;
    } else if (p < d) {
        stats.dealerWins++;
        printIfInteractive("Dealer Win");
    } else {
        stats.draw++;
        printIfInteractive("Push");
        stats.bank += player.bet;
    }
}

void turnFull(std::vector<int>& deck, std::vector<int>& dealer, std::mt19937& rng, const float bet) {
    std::vector<Hand> hands;
    stats.bank -= bet; // upfront
    stats.totalBet += bet;
    hands.push_back(makeHand(bet));

    dealInitialCards(deck, hands[0], dealer, rng, bet);
    stats.hands++;

    if (detectBlackjacks(deck, hands[0], dealer, bet)) return;

    if constexpr (INTERACTIVE) {
        std::cout << "Round " << stats.hands << std::endl;
        interactiveHand(deck,hands,dealer);
    } else {
        playPlayerHands(deck, hands, dealer);
    }
    playDealerHand(deck, dealer);

    for (size_t i = 0; i < hands.size(); ++i) {
        if (INTERACTIVE) std::cout << "Hand " << (i + 1) << std::endl;
        resolveHand(hands[i], dealer);
    }
}
// END TURN ACTIONS

int main() {
    printGlobalVars();

    std::vector<int> deck;
    deck.reserve(52 * NUMBER_DECKS);

    std::random_device rd;
    std::mt19937 rng(rd());

    shuffleDeck(deck, rng);
    std::vector<int> handDealer = initHand();

    for (int i = 0; i < NUMBER_HANDS; i++) {
        float bet;
        if constexpr (CARD_COUNTING == true) {
            bet = betFromTrueCount() * DEFAULT_BET;
        } else bet = DEFAULT_BET;

        if (stats.bank < bet) {
            printStats();
            return 0;
        }

        turnFull(deck, handDealer, rng, bet);
    }

    printStats();
    return 0;
}
