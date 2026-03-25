#include "actions.h"

const Action HARD[22][12] = {
    {S, S, S, S, S, S, S, S, S, S, S, S}, /* 0-4 Unused */
    {S, S, S, S, S, S, S, S, S, S, S, S}, {S, S, S, S, S, S, S, S, S, S, S, S},
    {S, S, S, S, S, S, S, S, S, S, S, S}, {S, S, S, S, S, S, S, S, S, S, S, S},

    {S, S, H, H, H, H, H, H, H, H, H, H}, /* 5 */
    {S, S, H, H, H, H, H, H, H, H, H, H}, /* 6 */
    {S, S, H, H, H, H, H, H, H, H, H, H}, /* 7 */
    {S, S, H, H, H, H, H, H, H, H, H, H}, /* 8 */
    {S, S, H, D, D, D, D, H, H, H, H, H}, /* 9 */
    {S, S, D, D, D, D, D, D, D, D, H, H}, /* 10 */
    {S, S, D, D, D, D, D, D, D, D, D, D}, /* 11 */
    {S, S, H, H, S, S, S, H, H, H, H, H}, /* 12 */
    {S, S, S, S, S, S, S, H, H, H, H, H}, /* 13 */
    {S, S, S, S, S, S, S, H, H, H, H, H}, /* 14 */
    {S, S, S, S, S, S, S, H, H, H, H, H}, /* 15 */
    {S, S, S, S, S, S, S, H, H, H, H, H}, /* 16 */
    {S, S, S, S, S, S, S, S, S, S, S, S}, /* 17 */
    {S, S, S, S, S, S, S, S, S, S, S, S}, /* 18 */
    {S, S, S, S, S, S, S, S, S, S, S, S}, /* 19 */
    {S, S, S, S, S, S, S, S, S, S, S, S}, /* 20 */
    {S, S, S, S, S, S, S, S, S, S, S, S}  /* 21 */
};

const Action SOFT[22][12] = {
    {S, S, S, S, S, S, S, S, S, S, S, S}, /* 0–12 Unused */
    {S, S, S, S, S, S, S, S, S, S, S, S}, {S, S, S, S, S, S, S, S, S, S, S, S},
    {S, S, S, S, S, S, S, S, S, S, S, S}, {S, S, S, S, S, S, S, S, S, S, S, S},
    {S, S, S, S, S, S, S, S, S, S, S, S}, {S, S, S, S, S, S, S, S, S, S, S, S},
    {S, S, S, S, S, S, S, S, S, S, S, S}, {S, S, S, S, S, S, S, S, S, S, S, S},
    {S, S, S, S, S, S, S, S, S, S, S, S}, {S, S, S, S, S, S, S, S, S, S, S, S},
    {S, S, S, S, S, S, S, S, S, S, S, S}, {S, S, S, S, S, S, S, S, S, S, S, S},

    {S, S, H, H, H, D, D, H, H, H, H, H}, /* 13 A2 */
    {S, S, H, H, H, D, D, H, H, H, H, H}, /* 14 A3 */
    {S, S, H, H, D, D, D, H, H, H, H, H}, /* 15 A4 */
    {S, S, H, H, D, D, D, H, H, H, H, H}, /* 16 A5 */
    {S, S, H, D, D, D, D, H, H, H, H, H}, /* 17 A6 */
    {S, S, S, D, D, D, D, S, H, H, H, H}, /* 18 A7 */
    {S, S, S, S, S, S, S, S, S, S, S, S}, /* 19 A8 */
    {S, S, S, S, S, S, S, S, S, S, S, S}, /* 20 A9 */
    {S, S, S, S, S, S, S, S, S, S, S, S}  /* 21 */
};

const Action PAIR[12][12] = {
    {S, S, S, S, S, S, S, S, S, S, S, S}, /* 0-1 Unused */
    {S, S, S, S, S, S, S, S, S, S, S, S},

    {S, S, P, P, P, P, P, P, H, H, H, H}, /* 2,2 */
    {S, S, P, P, P, P, P, P, H, H, H, H}, /* 3,3 */
    {S, S, H, H, H, P, P, H, H, H, H, H}, /* 4,4 */
    {S, S, D, D, D, D, D, D, D, D, H, H}, /* 5,5 */
    {S, S, P, P, P, P, P, H, H, H, H, H}, /* 6,6 */
    {S, S, P, P, P, P, P, P, H, H, H, H}, /* 7,7 */
    {S, S, P, P, P, P, P, P, P, P, P, P}, /* 8,8 */
    {S, S, P, P, P, P, P, S, P, P, S, S}, /* 9,9 */
    {S, S, S, S, S, S, S, S, S, S, S, S}, /*10,10*/
    {S, S, P, P, P, P, P, P, P, P, P, P}  /* A,A */
};

Action getAction(const int total, const int dealerUp, const bool isSoft,
                 const bool isPair, const int pairRank) {
  if (isPair)
    return PAIR[pairRank][dealerUp];
  if (isSoft)
    return SOFT[total][dealerUp];
  return HARD[total][dealerUp];
}