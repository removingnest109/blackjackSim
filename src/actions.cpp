#include "actions.h"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

// Active strategy chart, initialised to the canonical basic-strategy default.
// Overridden before simulation by the CLI (--strategy) or the GUI editor.
StrategyTable gStrategy = kBasicStrategy;

namespace {

std::optional<Action> codeToAction(const std::string &code) {
  if (code.size() != 1)
    return std::nullopt;
  switch (code[0]) {
  case 'H':
    return Action::Hit;
  case 'S':
    return Action::Stand;
  case 'D':
    return Action::Double;
  case 'P':
    return Action::Split;
  case 'R':
    return Action::Surrender;
  default:
    return std::nullopt;
  }
}

const char *actionToCode(Action a) {
  switch (a) {
  case Action::Hit:
    return "H";
  case Action::Double:
    return "D";
  case Action::Split:
    return "P";
  case Action::Stand:
    return "S";
  case Action::Surrender:
    return "R";
  }
  return "S";
}

// Validate one grid's shape (rows x 12) and cell codes into a temporary before
// any of it is copied into the target, so a malformed grid never half-applies.
template <std::size_t Rows>
bool parseGrid(const nlohmann::json &j, const char *name,
               Action (&out)[Rows][12]) {
  auto it = j.find(name);
  if (it == j.end() || !it->is_array() || it->size() != Rows) {
    std::cerr << "strategy: \"" << name << "\" must be an array of " << Rows
              << " rows\n";
    return false;
  }
  Action tmp[Rows][12];
  for (std::size_t r = 0; r < Rows; ++r) {
    const nlohmann::json &row = (*it)[r];
    if (!row.is_array() || row.size() != 12) {
      std::cerr << "strategy: \"" << name << "\" row " << r
                << " must have 12 columns\n";
      return false;
    }
    for (std::size_t c = 0; c < 12; ++c) {
      if (!row[c].is_string()) {
        std::cerr << "strategy: \"" << name << "\" [" << r << "][" << c
                  << "] must be a string code\n";
        return false;
      }
      const auto a = codeToAction(row[c].get<std::string>());
      if (!a) {
        std::cerr << "strategy: \"" << name << "\" [" << r << "][" << c
                  << "] has invalid code \"" << row[c].get<std::string>()
                  << "\"\n";
        return false;
      }
      tmp[r][c] = *a;
    }
  }
  for (std::size_t r = 0; r < Rows; ++r)
    for (std::size_t c = 0; c < 12; ++c)
      out[r][c] = tmp[r][c];
  return true;
}

template <std::size_t Rows>
void gridToJson(const Action (&grid)[Rows][12], nlohmann::json &out) {
  out = nlohmann::json::array();
  for (std::size_t r = 0; r < Rows; ++r) {
    nlohmann::json row = nlohmann::json::array();
    for (std::size_t c = 0; c < 12; ++c)
      row.push_back(actionToCode(grid[r][c]));
    out.push_back(std::move(row));
  }
}

} // namespace

nlohmann::json strategyToJson(const StrategyTable &t) {
  nlohmann::json j;
  gridToJson(t.hard, j["hard"]);
  gridToJson(t.soft, j["soft"]);
  gridToJson(t.pair, j["pair"]);
  return j;
}

bool strategyFromJson(const nlohmann::json &j, StrategyTable &out) {
  if (!j.is_object()) {
    std::cerr << "strategy: root must be a JSON object\n";
    return false;
  }
  // Parse into a scratch table first so a failure on a later grid can't leave
  // `out` partly overwritten.
  StrategyTable scratch;
  if (!parseGrid(j, "hard", scratch.hard) ||
      !parseGrid(j, "soft", scratch.soft) ||
      !parseGrid(j, "pair", scratch.pair))
    return false;
  out = scratch;
  return true;
}

bool loadStrategyFromJson(const std::string &path, StrategyTable &out) {
  std::ifstream f(path);
  if (!f) {
    std::cerr << "strategy: cannot open file \"" << path << "\"\n";
    return false;
  }
  nlohmann::json j;
  try {
    f >> j;
  } catch (const nlohmann::json::exception &e) {
    std::cerr << "strategy: failed to parse \"" << path << "\": " << e.what()
              << "\n";
    return false;
  }
  return strategyFromJson(j, out);
}
