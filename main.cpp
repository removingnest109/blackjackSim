#include <algorithm>
#include <iostream>
#include <random>
#include <vector>

constexpr int NUMBER_DECKS = 6;
constexpr int NUMBER_HANDS = 10000000;
constexpr int RESHUFFLE_CARD = 260;
constexpr bool DEALER_HIT_ON_SOFT_17 = false;
constexpr float PLAYER_STARTING_BANK = 1000000;
constexpr bool CARD_COUNTING = true;
constexpr float DEFAULT_BET = 100.0;
constexpr bool INTERACTIVE = false;

struct stats {
    uint32_t playerWins = 0;
    uint32_t dealerWins = 0;
    uint32_t draw = 0;
    uint32_t playerBlackjacks = 0;
    uint32_t dealerBlackjacks = 0;
    uint32_t shuffles = 0;
    uint32_t cardsDealt = 0;
    float bank = PLAYER_STARTING_BANK;
    uint32_t hands = 0;
    uint32_t splits = 0;
    uint32_t doubles = 0;
    uint32_t cardsSinceShuffle = 0;
    int runningCount = 0;
    int trueCount = 0;
    float totalBet = 0;
};

struct Hand {
    std::vector<int> cards;
    float bet;
    bool doubled = false;
    bool splitAces = false;
};


stats stats;

// PRINT
void printGlobalVars() {
    std::cout << "Number of decks: " << NUMBER_DECKS << std::endl;
    std::cout << "Number of hands being played: " << NUMBER_HANDS << std::endl;
    std::cout << "Reshuffle at card " << RESHUFFLE_CARD << std::endl;
    std::cout << "Player starting bank: " << static_cast<int>(PLAYER_STARTING_BANK) << std::endl;
    if constexpr (DEALER_HIT_ON_SOFT_17) {
        std::cout << "Dealer hits on soft 17\n";
    } else {
        std::cout << "Dealer stands on soft 17\n";
    }
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

    std::cout << "player bank: " << static_cast<int>(stats.bank) << std::endl;

    const float profit = stats.bank - PLAYER_STARTING_BANK;
    const float evPerHand = profit / stats.hands;
    const float evPercent = profit / stats.totalBet;

    std::cout << "EV per hand: " << evPerHand << " $" << std::endl;
    std::cout << "EV percentage: " << evPercent * 100 << "%" << std::endl;
}

void printIfInteractive(const std::string& msg) {
    if constexpr (INTERACTIVE) {
        std::cout << msg << '\n';
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

    if (stats.cardsSinceShuffle > RESHUFFLE_CARD) {
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
    if (stats.trueCount == 1) return 2.0f;
    if (stats.trueCount == 2) return 4.0f;
    if (stats.trueCount == 3) return 8.0f;
    if (stats.trueCount == 4) return 12.0f;
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

// PLAYER DECISIONS
bool shouldSplit(const int card, const int dealerUp) {
    switch (card) {
        case 11: return true;
        case 8:  return true;
        case 9:  return dealerUp != 7 && dealerUp != 10 && dealerUp != 11;
        case 7:  return dealerUp <= 7;
        case 6:  return dealerUp <= 6;
        case 4:  return dealerUp == 5 || dealerUp == 6;
        case 3:
        case 2:  return dealerUp <= 7;
        default: return false;
    }
}

// TODO shouldHit, shouldStand

bool shouldDoubleHard(const int value, const int dealerUp) {
    if (value == 11) return true;
    if (value == 10) return dealerUp <= 9;
    if (value == 9)  return dealerUp >= 3 && dealerUp <= 6;
    return false;
}

bool shouldDoubleSoft(const int value, const int dealerUp) {
    switch (value) {
        case 13:
        case 14: return dealerUp >= 5 && dealerUp <= 6;
        case 15:
        case 16: return dealerUp >= 4 && dealerUp <= 6;
        case 17: return dealerUp >= 3 && dealerUp <= 6;
        case 18: return dealerUp >= 3 && dealerUp <= 6;
        default: return false;
    }
}
// END PLAYER DECISIONS

// TURN ACTIONS
void playPlayerHands(
    std::vector<int>& deck, std::vector<Hand>& hands, const std::vector<int>& dealer) {
    for (size_t i = 0; i < hands.size(); ++i) {
        while (true) {
            Hand& hand = hands[i];
            const int dealerUp = dealer[0];
            const int value = calculateHandValue(hand.cards);

            // HAVE TO STAND ON SPLIT ACES
            if (hand.splitAces) {
                break;
            }

            // SPLIT
            if (hand.cards.size() == 2 && hand.cards[0] == hand.cards[1] && hands.size() < 4) {
                if (shouldSplit(hand.cards[0], dealerUp)) {
                    hands.push_back(split(deck, hand));
                    continue;
                }
            }

            // SOFT HAND
            if (isSoftHand(hand.cards)) {
                if (shouldDoubleSoft(value, dealerUp)) {
                    doubleDown(deck, hand);
                    break;
                }
                if (value <= 17) {
                    drawCard(deck, hand.cards, true);
                    continue;
                }
                if (value == 18) {
                    if (dealerUp >= 9) {
                        drawCard(deck, hand.cards, true);
                        continue;
                    }
                    break;
                }
                break; // soft 19+
            }

            // HARD HAND
            if (!isSoftHand(hand.cards)) {
                if (shouldDoubleHard(value, dealerUp)) {
                    doubleDown(deck, hand);
                    break;
                }
                if (value <= 11) {
                    drawCard(deck, hand.cards, true);
                    continue;
                }
                if (value == 12) {
                    if (dealerUp >= 4 && dealerUp <= 6) break;
                    drawCard(deck, hand.cards, true);
                    continue;
                }
                if (value >= 13 && value <= 16) {
                    if (dealerUp <= 6) break;
                    drawCard(deck, hand.cards, true);
                    continue;
                }
                break; // 17+
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
