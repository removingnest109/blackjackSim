#include "interactive.h"
#include <iostream>

#include "blackjack.h"

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
