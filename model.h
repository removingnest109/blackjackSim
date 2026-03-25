#pragma once

#include <array>
#include <cstdint>
#include <vector>

struct Hand {
    std::array<int, 22> cards{};
    int cardCount = 0;
    int value = 0;
    int aceCount = 0;
    int64_t bet{};
    bool doubled = false;
    bool splitAces = false;

    bool isSoft() const { return value <= 21 && aceCount > 0; }
};

struct Deck {
    std::vector<int> cards;
    int size = 0;
};

extern const int8_t countTable[12];