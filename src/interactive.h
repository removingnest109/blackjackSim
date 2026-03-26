#pragma once

#include "model.h"
#include "stats.h"
#include <vector>

void interactiveHand(std::vector<int> &deck, Hand hands[], int &handCount,
                     const Hand &dealer, Stats &stats);