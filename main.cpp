#include <iostream>
#include <random>
#include <vector>

constexpr int NUMBER_DECKS = 1;
constexpr int NUMBER_HANDS = 100;
constexpr int MIN_BET = 10;
constexpr int MAX_BET = 1000;
constexpr int RESHUFFLE_CARD = 260;
constexpr bool DEALER_HIT_ON_SOFT_17 = false;

void printGlobalVars() {
    std::cout << "Number of decks: " << NUMBER_DECKS << "\n";
    std::cout << "Number of hands being played: " << NUMBER_HANDS << "\n";
    std::cout << "Min bet: " << MIN_BET << "\n";
    std::cout << "Max bet: " << MAX_BET << "\n";
    std::cout << "Reshuffle at card " << RESHUFFLE_CARD << "\n";
    if constexpr (DEALER_HIT_ON_SOFT_17) {
        std::cout << "Dealer hits on soft 17\n";
    } else {
        std::cout << "Dealer stands on soft 17\n";
    }
}

std::vector<int> initDeck() {
    std::vector<int> deck;
    deck.reserve(NUMBER_DECKS);

    for (int d = 0; d < NUMBER_DECKS; ++d) { // do once per deck
        // Numbered cards
        for (int value = 2; value <= 10; ++value) { // for each value 2-10
            for (int count = 0; count < 4; ++count) { // 4x suits per card
                deck.push_back(value);
            }
        }
        // Face cards
        for (int count = 0; count < 4 * 3; ++count) { // 3x face cards, 4x suits per card
            deck.push_back(10);
        }
        // Aces
        for (int count = 0; count < 4; ++count) { // 4x suits of ace
            deck.push_back(11);
        }
    }

    return deck;
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
        total -= 10; // Ace becomes 1 instead of 11
        aces--;
    }

    return total;
}

void printCards(const std::vector<int>& cards, const std::string& name) {
    std::cout << name << ": ";
    for (const int i : cards) {
        std::cout << i << " ";
    }
    std::cout << "\n"; // newline after all cards printed
}

void printHandValue(const std::vector<int>& hand) {
    std::cout << "Hand value: " << calculateHandValue(hand) << "\n";
}

void shuffleDeck(std::vector<int>& deck, std::mt19937 rng) {
    std::shuffle(deck.begin(), deck.end(), rng);
}

void drawCard(std::vector<int>& deck, std::vector<int>& hand) {
    const int card = deck.back();
    deck.pop_back();
    hand.push_back(card);
}

void dealInitialCards(std::vector<int>& deck,std::vector<int>& handPlayer, std::vector<int>& handDealer) {
    drawCard(deck, handDealer);
    drawCard(deck, handPlayer);
    drawCard(deck, handDealer);
    drawCard(deck, handPlayer);
}

int main() {
    std::mt19937 rng(123456);

    printGlobalVars();

    std::vector<int> deck = initDeck();
    std::vector<int> handPlayer = initHand();
    std::vector<int> handDealer = initHand();

    shuffleDeck(deck, rng);
    printCards(deck, "Deck");

    dealInitialCards(deck, handPlayer, handDealer);
    printCards(deck, "Deck after dealing initial cards");

    printCards(handPlayer, "Player Hand");
    printHandValue(handPlayer);

    printCards(handDealer, "Dealer Hand");
    printHandValue(handDealer);

    return 0;
}
