#include <algorithm>
#include <iostream>
#include <random>
#include <vector>

constexpr int NUMBER_DECKS = 6;
constexpr int NUMBER_HANDS = 10000000;
constexpr int RESHUFFLE_CARD = 260;
constexpr bool DEALER_HIT_ON_SOFT_17 = false;
int COUNT = 0;

struct stats {
    int playerWins = 0;
    int dealerWins = 0;
    int draw = 0;
    int playerBlackjacks = 0;
    int dealerBlackjacks = 0;
    int shuffles = 0;
    int cardsDealt = 0;
};

stats stats;

void printGlobalVars() {
    std::cout << "Number of decks: " << NUMBER_DECKS << "\n";
    std::cout << "Number of hands being played: " << NUMBER_HANDS << "\n";
    std::cout << "Reshuffle at card " << RESHUFFLE_CARD << " or when under 20 cards remaining in deck\n";
    if constexpr (DEALER_HIT_ON_SOFT_17) {
        std::cout << "Dealer hits on soft 17\n";
    } else {
        std::cout << "Dealer stands on soft 17\n";
    }
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

std::vector<int> initHand() {
    std::vector<int> hand;
    hand.reserve(22); // most possible number of cards in a hand
    return hand;
}

int calculateHandValue(const std::vector<int>& hand) {
    int total = 0;
    int aces = 0;

    for (const int card : hand) {
        total += card;
        if (card == 11) aces++;
    }

    while (total > 21 && aces > 0) {
        total -= 10;
        aces--;
    }

    return total;
}

bool isSoftHand(const std::vector<int>& hand) {
    int total = 0;
    int aces = 0;

    for (const int card : hand) {
        total += card;
        if (card == 11) aces++;
    }

    return (total <= 21 && aces > 0);
}

bool isBlackjack(const std::vector<int>& hand) {
    return hand.size() == 2 && calculateHandValue(hand) == 21;
}

void shuffleDeck(std::vector<int>& deck, std::mt19937& rng) {
    initDeck(deck);
    std::ranges::shuffle(deck, rng);
    stats.shuffles++;
    COUNT = 0;
}

void drawCard(std::vector<int>& deck, std::vector<int>& hand) {
    const int card = deck.back();
    deck.pop_back();
    hand.push_back(card);
    COUNT++;
    stats.cardsDealt++;
}

void dealInitialCards(std::vector<int>& deck, std::vector<int>& handPlayer, std::vector<int>& handDealer, std::mt19937& rng) {
    handPlayer.clear();
    handDealer.clear();

    if (COUNT > (NUMBER_DECKS * 52) - 20) {
        shuffleDeck(deck, rng);
    }

    if (COUNT > RESHUFFLE_CARD) {
        shuffleDeck(deck, rng);
    }

    drawCard(deck, handDealer);
    drawCard(deck, handPlayer);
    drawCard(deck, handDealer);
    drawCard(deck, handPlayer);
}

void turnDealer(std::vector<int>& deck, std::vector<int>& hand) {
    while (true) {
        const int value = calculateHandValue(hand);

        if (value < 17) {
            drawCard(deck, hand);
            continue;
        }

        if (value == 17 && isSoftHand(hand) && DEALER_HIT_ON_SOFT_17) {
            drawCard(deck, hand);
            continue;
        }

        break;
    }
}

void turnPlayer(std::vector<int>& deck, std::vector<int>& handPlayer, const std::vector<int>& handDealer) {
    while (true) {
        const int value = calculateHandValue(handPlayer);

        if (isSoftHand(handPlayer)) {
            if (value < 18) {
                drawCard(deck, handPlayer);
                continue;
            }

            if (value == 18 && handDealer[0] > 8) {
                drawCard(deck, handPlayer);
                continue;
            }

            break;
        }

        if (value < 12) {
            drawCard(deck, handPlayer);
            continue;
        }

        if (value > 16) {
            break;
        }

        if (handDealer[0] > 7) {
            drawCard(deck, handPlayer);
            continue;
        }

        break;
    }
}

void checkWinner(const std::vector<int>& handPlayer, const std::vector<int>& handDealer) {
    const int valuePlayer = calculateHandValue(handPlayer);
    const int valueDealer = calculateHandValue(handDealer);

    if (valuePlayer > 21) {
        stats.dealerWins++;
    } else if (valueDealer > 21) {
        stats.playerWins++;
    } else if (valuePlayer == valueDealer) {
        stats.draw++;
    } else if (valuePlayer > valueDealer) {
        stats.playerWins++;
    } else if (valuePlayer < valueDealer) {
        stats.dealerWins++;
    } else {
        throw std::invalid_argument("something wrong");
    }
}

void turnFull(std::vector<int>& deck, std::vector<int>& handPlayer, std::vector<int>& handDealer, std::mt19937& rng) {
    dealInitialCards(deck, handPlayer, handDealer, rng);
    if (isBlackjack(handDealer) && isBlackjack(handPlayer)) {
        stats.draw++;
    } else if (isBlackjack(handDealer)) {
        stats.dealerWins++;
        stats.dealerBlackjacks++;
    } else if (isBlackjack(handPlayer)) {
        stats.playerWins++;
        stats.playerBlackjacks++;
    } else {
        turnPlayer(deck, handPlayer, handDealer);
        turnDealer(deck, handDealer);
        checkWinner(handPlayer, handDealer);
    }
}

void printStats() {
    std::cout << stats.dealerWins << " Dealer Wins" << std::endl;
    std::cout << stats.dealerBlackjacks << " Dealer Blackjacks" << std::endl;

    std::cout << stats.draw << " Draw" << std::endl;

    std::cout << stats.playerWins << " Player Wins" << std::endl;
    std::cout << stats.playerBlackjacks << " Player Blackjacks" << std::endl;

    std::cout << stats.shuffles << " Shuffles" << std::endl;
    std::cout << stats.cardsDealt << " Cards dealt" << std::endl;

    const float winPercent = (static_cast<float>(stats.playerWins) / (static_cast<float>(stats.playerWins) + static_cast<float>(stats.dealerWins)));
    std::cout << "player win percentage excl draws: " << winPercent * 100 << "%" << std::endl;
}

int main() {
    printGlobalVars();

    std::vector<int> deck;
    deck.reserve(52 * NUMBER_DECKS);

    std::random_device rd;
    std::mt19937 rng(rd());

    shuffleDeck(deck, rng);

    std::vector<int> handPlayer = initHand();
    std::vector<int> handDealer = initHand();

    for (int i = 0; i < NUMBER_HANDS; i++) {
        turnFull(deck, handPlayer, handDealer, rng);
    }

    printStats();
    return 0;
}
