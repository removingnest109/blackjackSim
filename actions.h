#pragma once

enum class Action { Hit, Double, Split, Stand };

constexpr auto H = Action::Hit;
constexpr auto D = Action::Double;
constexpr auto P = Action::Split;
constexpr auto S = Action::Stand;

extern const Action HARD[22][12];
extern const Action SOFT[22][12];
extern const Action PAIR[12][12];

Action getAction(int total, int dealerUp, bool isSoft, bool isPair,
                 int pairRank);