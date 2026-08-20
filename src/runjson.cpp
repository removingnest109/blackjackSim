#include "runjson.h"

#include "config.h"
#include "monitor.h"
#include "series.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

nlohmann::json statsToJson(const Stats &st) {
  return {{"hands", st.hands},
          {"playerWins", st.playerWins},
          {"dealerWins", st.dealerWins},
          {"playerBlackjacks", st.playerBlackjacks},
          {"dealerBlackjacks", st.dealerBlackjacks},
          {"draw", st.draw},
          {"shuffles", st.shuffles},
          {"cardsDealt", st.cardsDealt},
          {"splits", st.splits},
          {"doubles", st.doubles},
          {"totalBet", st.totalBet},
          {"bank", st.bank}};
}

namespace {

std::string fmtInt(int64_t v) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v < 0 ? -v : v));
  std::string digits = buf;
  std::string out;
  const int len = static_cast<int>(digits.size());
  for (int i = 0; i < len; ++i) {
    if (i > 0 && (len - i) % 3 == 0)
      out += ',';
    out += digits[i];
  }
  return (v < 0 ? "-" : "") + out;
}

// Parameter summary matching the GUI's describeRun, built from the global
// config so imported CLI runs show the same tooltip as GUI runs.
std::string describeRun() {
  char buf[256];
  char bet[48];
  if (config.betPercentMode)
    std::snprintf(bet, sizeof(bet), "bet %.2f%% of bank", config.betPercent);
  else
    std::snprintf(bet, sizeof(bet), "bet %d", config.defaultBetSize);
  std::snprintf(
      buf, sizeof(buf),
      "%s hands/thread, %d decks, bank %s, %s, min bet %d, max bet %s,\n"
      "pen %.2f, %s, counting %s, debt %s, %d thread%s, %d player%s/table",
      fmtInt(config.numberHands).c_str(), config.numberDecks,
      fmtInt(config.startingBank).c_str(), bet, config.minimumBet,
      config.maximumBet == 0 ? "none" : std::to_string(config.maximumBet).c_str(),
      config.penetrationBeforeShuffle, config.dealerHitSoft17 ? "H17" : "S17",
      config.cardCounting ? "on" : "off", config.debtAllowed ? "on" : "off",
      config.threads, config.threads > 1 ? "s" : "", config.playersPerTable,
      config.playersPerTable > 1 ? "s" : "");
  std::string result = buf;
  if (config.cardCounting) {
    result += "\nbet curve: {";
    for (int i = 0; i < kBetCurveSize; ++i) {
      char entry[8];
      std::snprintf(entry, sizeof(entry), i + 1 < kBetCurveSize ? "%d," : "%d}",
                    config.betCurve[i]);
      result += entry;
    }
  }
  return result;
}

} // namespace

bool saveRunJson(const std::string &path, const SimMonitor &monitor,
                 const Stats &result, double elapsed) {
  // Rebuild per-series bank timelines from the probes using the same
  // table*players + player layout the GUI's pollSim consumes.
  std::vector<std::vector<double>> xs, ys;
  for (const auto &probePtr : monitor.probes) {
    const ThreadProbe &probe = *probePtr;
    const int N = probe.playerCount;
    const int n = probe.sampleCount.load(std::memory_order_acquire);
    for (int p = 0; p < N; ++p) {
      std::vector<double> sx, sy;
      sx.reserve(n);
      sy.reserve(n);
      for (int i = 0; i < n; ++i) {
        sx.push_back(static_cast<double>(probe.sampleHands[i]));
        sy.push_back(static_cast<double>(
            probe.samplePlayerBanks[static_cast<size_t>(i) * N + p]));
      }
      xs.push_back(std::move(sx));
      ys.push_back(std::move(sy));
    }
  }

  std::vector<double> avgX, avgY;
  buildAverageSeries(xs, ys, avgX, avgY);

  double worstDrawdown = 0.0;
  for (const auto &series : ys)
    worstDrawdown = std::max(worstDrawdown, maxDrawdown(series));

  int bankrupt = 0;
  double best = 0.0, worst = 0.0;
  bool first = true;
  for (const auto &series : ys) {
    if (series.empty())
      continue;
    const double v = series.back();
    if (v < static_cast<double>(config.minimumBet))
      ++bankrupt;
    if (first || v > best)
      best = v;
    if (first || v < worst)
      worst = v;
    first = false;
  }

  nlohmann::json jr;
  jr["label"] = "cli run";
  jr["desc"] = describeRun();
  jr["startBank"] = std::max(1, config.startingBank);
  jr["elapsed"] = elapsed;
  jr["stopped"] = false; // the CLI always runs a full simulation
  jr["stats"] = statsToJson(result);
  jr["bankruptedThreads"] = bankrupt;
  jr["bestEndBank"] = best;
  jr["worstEndBank"] = worst;
  jr["threadCount"] = std::max<int>(1, static_cast<int>(config.threads));
  jr["playersPerTable"] = std::max(1, config.playersPerTable);
  jr["worstDrawdown"] = worstDrawdown;
  jr["avg"] = {{"x", avgX}, {"y", avgY}};
  jr["threads"] = nlohmann::json::array();
  for (size_t t = 0; t < xs.size(); ++t)
    jr["threads"].push_back({{"x", xs[t]}, {"y", ys[t]}});

  nlohmann::json root;
  root["version"] = 1;
  root["runs"] = nlohmann::json::array();
  root["runs"].push_back(std::move(jr));

  std::ofstream f(path.c_str());
  if (!f)
    return false;
  f << root.dump();
  return true;
}
