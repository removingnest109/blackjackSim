#include <algorithm>
#include <iostream>
#include <random>
#include <vector>

constexpr int NUMBER_DECKS = 6;
constexpr int NUMBER_HANDS = 1000000;
constexpr int RESHUFFLE_CARD = 260;
constexpr bool DEALER_HIT_ON_SOFT_17 = false;
constexpr int PLAYER_STARTING_BANK = 100000;
int COUNT = 0;

constexpr int BET_SIZE = 10;

struct stats {
    uint32_t playerWins = 0;
    uint32_t dealerWins = 0;
    uint32_t draw = 0;
    uint32_t playerBlackjacks = 0;
    uint32_t dealerBlackjacks = 0;
    uint32_t shuffles = 0;
    uint32_t cardsDealt = 0;
    uint32_t bank = PLAYER_STARTING_BANK;
    uint32_t hands = 0;
    uint32_t splits = 0;
    uint32_t doubles = 0;
};

stats stats;

void printGlobalVars() {
    std::cout << "Number of decks: " << NUMBER_DECKS << "\n";
    std::cout << "Number of hands being played: " << NUMBER_HANDS << "\n";
    std::cout << "Reshuffle at card " << RESHUFFLE_CARD << "\n";
    std::cout << "Player starting bank: " << PLAYER_STARTING_BANK << "\n";
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

void checkWinner(const std::vector<int>& handPlayer, const std::vector<int>& handDealer) {
    const int valuePlayer = calculateHandValue(handPlayer);
    const int valueDealer = calculateHandValue(handDealer);

    if (valuePlayer > 21) {
        stats.dealerWins++;
        stats.bank = stats.bank - 10;
    } else if (valueDealer > 21) {
        stats.playerWins++;
        stats.bank = stats.bank + 10;
    } else if (valuePlayer == valueDealer) {
        stats.draw++;
    } else if (valuePlayer > valueDealer) {
        stats.playerWins++;
        stats.bank = stats.bank + 10;
    } else if (valuePlayer < valueDealer) {
        stats.dealerWins++;
        stats.bank = stats.bank - 10;
    } else {
        throw std::invalid_argument("something wrong");
    }
}

void turnPlayer(std::vector<int>& deck, std::vector<int>& handPlayer, const std::vector<int>& handDealer) {
    while (true) {
        const int value = calculateHandValue(handPlayer);

        if (isSoftHand(handPlayer)) {
            if (value == 12 && handPlayer[0] == 11 && handPlayer[1] == 11) { // split aces
                std::vector<int> handSplit = initHand();
                const int card = handPlayer.back();
                handPlayer.pop_back();
                handSplit.push_back(card);
                turnPlayer(deck, handSplit, handDealer);
                checkWinner(handSplit, handDealer);
                stats.splits++;
            }
            if (value <= 17) {
                drawCard(deck, handPlayer);
                continue;
            }
            if (value == 18 && handDealer[0] > 8) {
                drawCard(deck, handPlayer);
                continue;
            }
            break;
        }


        if (value == 11 && handPlayer.size() == 2) {
            drawCard(deck, handPlayer);
            checkWinner(handPlayer, handDealer); //double by checking winner twice
            stats.doubles++;
            break;
        }
        if (value <= 11) {
            drawCard(deck, handPlayer);
            continue;
        }
        if (handPlayer[0] == 8 && handPlayer[1] == 8) { // split 8s
            std::vector<int> handSplit = initHand();
            const int card = handPlayer.back();
            handPlayer.pop_back();
            handSplit.push_back(card);
            turnPlayer(deck, handSplit, handDealer);
            checkWinner(handSplit, handDealer);
            stats.splits++;
            continue;
        }
        if (value > 16) {
            break;
        }
        if (handDealer[0] > 6) {
            drawCard(deck, handPlayer);
            continue;
        }
        break;
    }
}

void turnFull(std::vector<int>& deck, std::vector<int>& handPlayer, std::vector<int>& handDealer, std::mt19937& rng) {
    dealInitialCards(deck, handPlayer, handDealer, rng);
    if (isBlackjack(handDealer) && isBlackjack(handPlayer)) {
        stats.draw++;
    } else if (isBlackjack(handDealer)) {
        stats.dealerWins++;
        stats.dealerBlackjacks++;
        stats.bank = stats.bank - 10;
    } else if (isBlackjack(handPlayer)) {
        stats.playerWins++;
        stats.playerBlackjacks++;
        stats.bank = stats.bank + 15;
    } else {
        turnPlayer(deck, handPlayer, handDealer);
        turnDealer(deck, handDealer);
        checkWinner(handPlayer, handDealer);
    }
    stats.hands++;
}

void printStats() {
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

    const float winPercent = (static_cast<float>(stats.playerWins) / (static_cast<float>(stats.playerWins) + static_cast<float>(stats.dealerWins)));
    std::cout << "player win percentage excl draws: " << winPercent * 100 << "%" << std::endl;

    std::cout << "player bank: " << stats.bank << std::endl;
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
        if (stats.bank < BET_SIZE) {
            printStats();
            return 0;
        }
    }

    printStats();
    return 0;
}
