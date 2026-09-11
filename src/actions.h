#pragma once

// Appended, never reordered: existing chart literals use the named aliases
// below, but keeping Surrender last also keeps the numeric enum values stable.
enum class Action { Hit, Double, Split, Stand, Surrender };

constexpr auto H = Action::Hit;
constexpr auto D = Action::Double;
constexpr auto P = Action::Split;
constexpr auto S = Action::Stand;
constexpr auto R = Action::Surrender;

// A full basic-strategy chart. Charts are runtime data (see gStrategy) so they
// can be overridden from a file or edited in the GUI; kBasicStrategy below is
// the canonical default.
//
// Column index is the dealer upcard value (Ace = 11; columns 0-1 are unused
// padding). Row is the hand total (or, for pair, the pair's card rank).
struct StrategyTable {
  Action hard[22][12]; // [total][dealerUp]
  Action soft[22][12]; // [total][dealerUp]
  Action pair[12][12]; // [pairRank][dealerUp]
};

inline constexpr StrategyTable kBasicStrategy = {
    .hard =
        {
            {S, S, S, S, S, S, S, S, S, S, S, S}, /* 0-4 Unused */
            {S, S, S, S, S, S, S, S, S, S, S, S},
            {S, S, S, S, S, S, S, S, S, S, S, S},
            {S, S, S, S, S, S, S, S, S, S, S, S},
            {S, S, S, S, S, S, S, S, S, S, S, S},

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
        },
    .soft =
        {
            {S, S, S, S, S, S, S, S, S, S, S, S}, /* 0-12 Unused */
            {S, S, S, S, S, S, S, S, S, S, S, S},
            {S, S, S, S, S, S, S, S, S, S, S, S},
            {S, S, S, S, S, S, S, S, S, S, S, S},
            {S, S, S, S, S, S, S, S, S, S, S, S},
            {S, S, S, S, S, S, S, S, S, S, S, S},
            {S, S, S, S, S, S, S, S, S, S, S, S},
            {S, S, S, S, S, S, S, S, S, S, S, S},
            {S, S, S, S, S, S, S, S, S, S, S, S},
            {S, S, S, S, S, S, S, S, S, S, S, S},
            {S, S, S, S, S, S, S, S, S, S, S, S},
            {S, S, S, S, S, S, S, S, S, S, S, S},
            {S, S, S, S, S, S, S, S, S, S, S, S},

            {S, S, H, H, H, D, D, H, H, H, H, H}, /* 13 A2 */
            {S, S, H, H, H, D, D, H, H, H, H, H}, /* 14 A3 */
            {S, S, H, H, D, D, D, H, H, H, H, H}, /* 15 A4 */
            {S, S, H, H, D, D, D, H, H, H, H, H}, /* 16 A5 */
            {S, S, H, D, D, D, D, H, H, H, H, H}, /* 17 A6 */
            {S, S, S, D, D, D, D, S, H, H, H, H}, /* 18 A7 */
            {S, S, S, S, S, S, S, S, S, S, S, S}, /* 19 A8 */
            {S, S, S, S, S, S, S, S, S, S, S, S}, /* 20 A9 */
            {S, S, S, S, S, S, S, S, S, S, S, S}  /* 21 */
        },
    .pair =
        {
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
        }};

// Mutable, active chart consulted by getAction. Written once before any worker
// thread starts (same lifecycle as the global config) and only read during
// simulation, so no locking is required.
extern StrategyTable gStrategy;

// Runtime lookup into gStrategy. Signature is unchanged from the old
// compile-time version; the flat array index is just as cheap at runtime.
inline Action getAction(int total, int dealerUp, bool isSoft, bool isPair,
                        int pairRank) {
  if (isPair)
    return gStrategy.pair[pairRank][dealerUp];
  if (isSoft)
    return gStrategy.soft[total][dealerUp];
  return gStrategy.hard[total][dealerUp];
}
