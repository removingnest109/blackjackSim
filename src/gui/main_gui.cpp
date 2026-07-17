// Blackjack Simulator GUI
// ImGui + ImPlot frontend over blackjack_core.

#include "config.h"
#include "monitor.h"
#include "simulation.h"
#include "stats.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include "misc/cpp/imgui_stdlib.h"
#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "portable-file-dialogs.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

// ---------------------------------------------------------------------------
// Shared "terminal" palette. A cool near-black surface stack, one muted slate
// accent reserved for interactive chrome and the live state, and green/red used
// only as financial signal on signed numbers. Kept in one place so the widget
// theme and the stats readout agree on every colour.
// ---------------------------------------------------------------------------

ImVec4 rgb(int r, int g, int b, float a = 1.0f) {
  return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a);
}

// Scale RGB while preserving alpha — used to derive darker line variants.
ImVec4 shade(const ImVec4 &c, float f) {
  return ImVec4(c.x * f, c.y * f, c.z * f, c.w);
}

namespace pal {
const ImVec4 accent = rgb(124, 146, 173);       // muted slate blue
const ImVec4 accentBright = rgb(158, 178, 202); // accent, hovered
const ImVec4 accentDim = rgb(88, 105, 130);     // accent, pressed
const ImVec4 accentSoft = rgb(124, 146, 173, 0.16f);
const ImVec4 pos = rgb(63, 185, 80);   // profit / positive
const ImVec4 neg = rgb(242, 109, 109); // loss / drawdown / negative
const ImVec4 text = rgb(201, 211, 224);
const ImVec4 textDim = rgb(108, 119, 137);
const ImVec4 bgWindow = rgb(14, 17, 22);
const ImVec4 bgChild = rgb(20, 25, 34);
const ImVec4 bgFrame = rgb(27, 34, 48);
const ImVec4 bgFrameHover = rgb(35, 44, 60);
const ImVec4 bgFrameActive = rgb(44, 55, 73);
const ImVec4 border = rgb(255, 255, 255, 0.07f);
} // namespace pal

// Fonts: Plex Sans for UI text, Plex Sans SemiBold for section headers, and
// Plex Mono for every numeric readout so figures line up in columns.
ImFont *gFontUI = nullptr;
ImFont *gFontHead = nullptr;
ImFont *gFontMono = nullptr;

struct GuiParams {
  int hands = 10000000;
  int decks = 6;
  int bank = 1000000;
  int bet = 10;
  bool betPercentMode = false;
  float betPercent = 1.0f;
  int minBet = 1;
  float penetration = 0.5f;
  int threads = 1;
  bool dealerHitSoft17 = false;
  bool cardCounting = false;
  bool debtAllowed = false;
  int betCurve[kBetCurveSize] = {1, 2, 3, 4, 5, 6};
};

struct RunRecord {
  int id = 0;
  std::string label; // "run 3", user-renamable
  std::string desc;  // parameter summary for tooltip
  int startBank = 1;
  Stats stats;
  double elapsed = 0.0;
  bool stopped = false;
  std::vector<std::vector<double>> xs, ys; // per-thread series
  std::vector<double> avgX, avgY;          // cross-thread average
  ImVec4 color = ImVec4(1, 1, 1, 1);    // thread lines
  ImVec4 avgColor = ImVec4(1, 1, 1, 1); // average line
  bool visible = true;
  bool showAvg = false;
};

struct AppState {
  GuiParams params;

  bool running = false;
  bool haveResult = false;
  bool wasStopped = false;
  bool normalize = false;
  int runCounter = 0;
  double finalDrawdown = 0.0;
  std::vector<RunRecord> history;

  std::string ioPath = "runs.json";
  std::string pngPath = "plot.png";
  std::string ioStatus;
  bool wantPlotExport = false;
  ImVec2 plotMin, plotMax;

  // Left column: two collapsible sections with a draggable height split.
  float paramsPanelH = 360.0f;
  bool paramsOpen = true;
  bool runsOpen = true;

  std::unique_ptr<SimMonitor> monitor;
  std::future<Stats> future;

  Clock::time_point startTime;
  Clock::time_point endTime;

  // Per-thread plot series.
  std::vector<std::vector<double>> xs;
  std::vector<std::vector<double>> ys;
  std::vector<int> consumed;

  Stats live;       // aggregate of latest per-thread snapshots
  Stats result;     // final merged stats
  GuiParams runParams; // params the current/last run started with
};

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

double divide(const int64_t num, const int64_t den) {
  return den == 0 ? 0.0
                  : static_cast<double>(num) / static_cast<double>(den);
}

std::string describeRun(const GuiParams &p) {
  char buf[256];
  char bet[48];
  if (p.betPercentMode)
    std::snprintf(bet, sizeof(bet), "bet %.2f%% of bank", p.betPercent);
  else
    std::snprintf(bet, sizeof(bet), "bet %d", p.bet);
  std::snprintf(buf, sizeof(buf),
                "%s hands/thread, %d decks, bank %s, %s, min bet %d,\n"
                "pen %.2f, %s, counting %s, debt %s, %d thread%s",
                fmtInt(p.hands).c_str(), p.decks, fmtInt(p.bank).c_str(), bet,
                p.minBet, p.penetration, p.dealerHitSoft17 ? "H17" : "S17",
                p.cardCounting ? "on" : "off", p.debtAllowed ? "on" : "off",
                p.threads, p.threads > 1 ? "s" : "");
  std::string result = buf;
  if (p.cardCounting) {
    result += "\nbet curve: {";
    for (int i = 0; i < kBetCurveSize; ++i) {
      char entry[8];
      std::snprintf(entry, sizeof(entry), i + 1 < kBetCurveSize ? "%d," : "%d}",
                    p.betCurve[i]);
      result += entry;
    }
  }
  return result;
}

double maxDrawdown(const std::vector<double> &series);

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

Stats statsFromJson(const nlohmann::json &j) {
  Stats st;
  st.hands = j.value("hands", int64_t(0));
  st.playerWins = j.value("playerWins", int64_t(0));
  st.dealerWins = j.value("dealerWins", int64_t(0));
  st.playerBlackjacks = j.value("playerBlackjacks", int64_t(0));
  st.dealerBlackjacks = j.value("dealerBlackjacks", int64_t(0));
  st.draw = j.value("draw", int64_t(0));
  st.shuffles = j.value("shuffles", int64_t(0));
  st.cardsDealt = j.value("cardsDealt", int64_t(0));
  st.splits = j.value("splits", int64_t(0));
  st.doubles = j.value("doubles", int64_t(0));
  st.totalBet = j.value("totalBet", int64_t(0));
  st.bank = j.value("bank", int64_t(0));
  return st;
}

std::string exportRuns(const AppState &s) {
  nlohmann::json root;
  root["version"] = 1;
  root["runs"] = nlohmann::json::array();
  for (size_t r = 0; r < s.history.size(); ++r) {
    const RunRecord &rec = s.history[r];
    nlohmann::json jr;
    jr["label"] = rec.label;
    jr["desc"] = rec.desc;
    jr["startBank"] = rec.startBank;
    jr["elapsed"] = rec.elapsed;
    jr["stopped"] = rec.stopped;
    jr["stats"] = statsToJson(rec.stats);
    jr["color"] = {rec.color.x, rec.color.y, rec.color.z, rec.color.w};
    jr["avgColor"] = {rec.avgColor.x, rec.avgColor.y, rec.avgColor.z,
                      rec.avgColor.w};
    jr["avg"] = {{"x", rec.avgX}, {"y", rec.avgY}};
    jr["threads"] = nlohmann::json::array();
    for (size_t t = 0; t < rec.xs.size(); ++t)
      jr["threads"].push_back({{"x", rec.xs[t]}, {"y", rec.ys[t]}});
    root["runs"].push_back(jr);
  }
  std::ofstream f(s.ioPath.c_str());
  if (!f)
    return "cannot open " + s.ioPath;
  f << root.dump();
  char msg[128];
  std::snprintf(msg, sizeof(msg), "exported %d runs to %s",
                static_cast<int>(s.history.size()), s.ioPath.c_str());
  return msg;
}

std::string importRuns(AppState &s) {
  std::ifstream f(s.ioPath.c_str());
  if (!f)
    return "cannot open " + s.ioPath;
  nlohmann::json root;
  try {
    f >> root;
  } catch (...) {
    return "failed to parse " + s.ioPath;
  }
  if (!root.contains("runs") || !root["runs"].is_array())
    return "no runs found in " + s.ioPath;
  int imported = 0;
  for (size_t r = 0; r < root["runs"].size(); ++r) {
    const nlohmann::json &jr = root["runs"][r];
    RunRecord rec;
    rec.id = ++s.runCounter;
    rec.label = jr.value("label", std::string("imported"));
    rec.desc = jr.value("desc", std::string(""));
    rec.startBank = std::max(1, jr.value("startBank", 1));
    rec.elapsed = jr.value("elapsed", 0.0);
    rec.stopped = jr.value("stopped", false);
    if (jr.contains("stats"))
      rec.stats = statsFromJson(jr["stats"]);
    if (jr.contains("color") && jr["color"].is_array() &&
        jr["color"].size() == 4)
      rec.color = ImVec4(jr["color"][0].get<float>(),
                         jr["color"][1].get<float>(),
                         jr["color"][2].get<float>(),
                         jr["color"][3].get<float>());
    else
      rec.color = ImPlot::GetColormapColor(rec.id - 1);
    if (jr.contains("avgColor") && jr["avgColor"].is_array() &&
        jr["avgColor"].size() == 4)
      rec.avgColor = ImVec4(jr["avgColor"][0].get<float>(),
                            jr["avgColor"][1].get<float>(),
                            jr["avgColor"][2].get<float>(),
                            jr["avgColor"][3].get<float>());
    else
      rec.avgColor = rec.color;
    if (jr.contains("avg")) {
      rec.avgX = jr["avg"].value("x", std::vector<double>());
      rec.avgY = jr["avg"].value("y", std::vector<double>());
    }
    if (jr.contains("threads")) {
      for (size_t t = 0; t < jr["threads"].size(); ++t) {
        rec.xs.push_back(jr["threads"][t].value("x", std::vector<double>()));
        rec.ys.push_back(jr["threads"][t].value("y", std::vector<double>()));
      }
    }
    s.history.push_back(std::move(rec));
    ++imported;
  }
  char msg[128];
  std::snprintf(msg, sizeof(msg), "imported %d runs from %s", imported,
                s.ioPath.c_str());
  return msg;
}

void startRun(AppState &s) {
  s.runCounter++;
  config.numberHands = s.params.hands;
  config.numberDecks = s.params.decks;
  config.startingBank = s.params.bank;
  config.defaultBetSize = s.params.bet;
  config.betPercentMode = s.params.betPercentMode;
  config.betPercent = s.params.betPercent;
  config.minimumBet = s.params.minBet;
  config.penetrationBeforeShuffle = s.params.penetration;
  config.dealerHitSoft17 = s.params.dealerHitSoft17;
  config.cardCounting = s.params.cardCounting;
  for (int i = 0; i < kBetCurveSize; ++i)
    config.betCurve[i] = s.params.betCurve[i];
  config.debtAllowed = s.params.debtAllowed;
  config.threads = static_cast<unsigned int>(s.params.threads);
  config.multiThread = s.params.threads > 1;

  s.runParams = s.params;
  s.monitor.reset(new SimMonitor(config.threads));
  s.xs.assign(config.threads, std::vector<double>());
  s.ys.assign(config.threads, std::vector<double>());
  s.consumed.assign(config.threads, 0);
  s.live = Stats();
  s.haveResult = false;
  s.wasStopped = false;
  s.startTime = Clock::now();
  s.running = true;

  SimMonitor *monitor = s.monitor.get();
  s.future = std::async(std::launch::async,
                        [monitor] { return runSim(monitor); });
}

void pollSim(AppState &s) {
  if (!s.running)
    return;

  Stats agg;
  for (size_t t = 0; t < s.monitor->probes.size(); ++t) {
    ThreadProbe &probe = *s.monitor->probes[t];
    const int n = probe.sampleCount.load(std::memory_order_acquire);
    for (int i = s.consumed[t]; i < n; ++i) {
      s.xs[t].push_back(static_cast<double>(probe.sampleHands[i]));
      s.ys[t].push_back(static_cast<double>(probe.sampleBank[i]));
    }
    s.consumed[t] = n;
    agg += probe.readLatest();
  }
  s.live = agg;

  if (s.future.valid() &&
      s.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    s.result = s.future.get();
    s.live = s.result;
    s.endTime = Clock::now();
    s.running = false;
    s.haveResult = true;

    double dd = 0.0;
    for (size_t t = 0; t < s.ys.size(); ++t)
      dd = std::max(dd, maxDrawdown(s.ys[t]));
    s.finalDrawdown = dd;

    // Archive the finished run so it can be overlaid against later runs.
    RunRecord rec;
    rec.id = s.runCounter;
    char lbl[32];
    std::snprintf(lbl, sizeof(lbl), "run %d", s.runCounter);
    rec.label = lbl;
    rec.desc = describeRun(s.runParams);
    rec.startBank = std::max(1, s.runParams.bank);
    rec.stats = s.result;
    rec.elapsed = std::chrono::duration<double>(s.endTime - s.startTime).count();
    rec.stopped = s.wasStopped;
    rec.showAvg = true; // show the cross-thread average by default
    rec.color = ImPlot::GetColormapColor(rec.id - 1);
    rec.avgColor = rec.color;
    size_t n = s.xs.empty() ? 0 : s.xs[0].size();
    for (size_t t = 1; t < s.xs.size(); ++t)
      n = std::min(n, s.xs[t].size());
    rec.avgX.reserve(n);
    rec.avgY.reserve(n);
    for (size_t i = 0; i < n; ++i) {
      double sumY = 0.0;
      for (size_t t = 0; t < s.ys.size(); ++t)
        sumY += s.ys[t][i];
      rec.avgX.push_back(s.xs[0][i]);
      rec.avgY.push_back(sumY / static_cast<double>(s.ys.size()));
    }
    rec.xs = std::move(s.xs);
    rec.ys = std::move(s.ys);
    s.xs.clear();
    s.ys.clear();
    s.history.push_back(std::move(rec));
  }
}

double maxDrawdown(const std::vector<double> &series) {
  double peak = series.empty() ? 0.0 : series[0];
  double worst = 0.0;
  for (size_t i = 0; i < series.size(); ++i) {
    peak = std::max(peak, series[i]);
    worst = std::max(worst, peak - series[i]);
  }
  return worst;
}

std::string fmtPct(double v) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.4f%%", v * 100.0);
  return buf;
}

std::string fmtDouble(double v, const char *suffix = "") {
  char buf[48];
  std::snprintf(buf, sizeof(buf), "%.4f%s", v, suffix);
  return buf;
}

// Uppercase, semibold section label — reads like a terminal panel heading.
void sectionHeader(const char *label) {
  ImGui::PushFont(gFontHead);
  ImGui::SeparatorText(label);
  ImGui::PopFont();
}

void drawParamsContent(AppState &s) {
  ImGui::BeginDisabled(s.running);
  ImGui::PushItemWidth(-FLT_MIN);

  ImGui::TextDisabled("Hands per thread");
  ImGui::SetItemTooltip("Number of hands each simulation thread plays.\n"
                        "More hands give more statistically reliable results.");
  ImGui::InputInt("##hands", &s.params.hands, 0, 0);
  s.params.hands = std::max(1, s.params.hands);

  ImGui::TextDisabled("Decks");
  ImGui::SetItemTooltip("Number of decks in the shoe.\n"
                        "Most casinos use 6 or 8 decks.");
  ImGui::SliderInt("##decks", &s.params.decks, 1, 12);

  ImGui::TextDisabled("Starting bank");
  ImGui::SetItemTooltip("Initial bankroll for each thread.");
  ImGui::InputInt("##bank", &s.params.bank, 0, 0);
  s.params.bank = std::max(1, s.params.bank);

  ImGui::TextDisabled("Bet sizing");
  ImGui::SetItemTooltip("How the wager amount is determined each hand.");
  if (ImGui::RadioButton("Raw bet", !s.params.betPercentMode))
    s.params.betPercentMode = false;
  ImGui::SetItemTooltip("Bet a fixed dollar amount every hand.");
  ImGui::SameLine();
  if (ImGui::RadioButton("% of bank", s.params.betPercentMode))
    s.params.betPercentMode = true;
  ImGui::SetItemTooltip("Bet a percentage of the current bankroll every hand\n"
                        "(proportional / Kelly-style staking).");

  if (s.params.betPercentMode) {
    ImGui::SliderFloat("##betpct", &s.params.betPercent, 0.01f, 100.0f,
                       "%.2f%%", ImGuiSliderFlags_Logarithmic);
    ImGui::SetItemTooltip("Percentage of current bankroll to wager each hand.");
  } else {
    ImGui::InputInt("##bet", &s.params.bet, 0, 0);
    ImGui::SetItemTooltip("Fixed dollar amount wagered each hand.");
    s.params.bet = std::max(1, s.params.bet);
  }

  ImGui::TextDisabled("Minimum bet");
  ImGui::SetItemTooltip("Floor bet enforced every hand.\n"
                        "In %% of bank mode the wager will never drop below this.");
  ImGui::InputInt("##minbet", &s.params.minBet, 0, 0);
  s.params.minBet = std::max(1, s.params.minBet);

  ImGui::TextDisabled("Shuffle penetration");
  ImGui::SetItemTooltip("Fraction of the shoe dealt before reshuffling.\n"
                        "0.5 = reshuffle at half the shoe, 1.0 = deal all cards.");
  ImGui::SliderFloat("##pen", &s.params.penetration, 0.05f, 1.0f, "%.2f");

  const int maxThreads =
      std::max(1u, std::thread::hardware_concurrency());
  ImGui::TextDisabled("Threads");
  ImGui::SetItemTooltip("Number of independent parallel simulations.\n"
                        "Results are aggregated across all threads.");
  ImGui::SliderInt("##threads", &s.params.threads, 1, maxThreads);

  ImGui::Spacing();
  ImGui::Checkbox("Dealer hits soft 17", &s.params.dealerHitSoft17);
  ImGui::SetItemTooltip("When enabled, the dealer must hit on a soft 17 (Ace + 6).\n"
                        "This rule increases the house edge slightly.");
  ImGui::Checkbox("Card counting", &s.params.cardCounting);
  ImGui::SetItemTooltip("Simulate Hi-Lo card counting with bet spreading.\n"
                        "The bet multiplier table below scales the wager by true count.");

  if (s.params.cardCounting) {
    if (ImGui::TreeNodeEx("Bet multiplier by true count",
                          ImGuiTreeNodeFlags_DefaultOpen |
                              ImGuiTreeNodeFlags_SpanAvailWidth)) {
      ImGui::SetItemTooltip("Multiply the base bet by this factor at each true count bucket.\n"
                            "Higher multipliers at positive counts exploit player advantage.");
      static const char *bucketLabels[kBetCurveSize] = {"<=0", "<=2", "<=3",
                                                        "<=4", "<=5", ">5"};
      for (int i = 0; i < kBetCurveSize; ++i) {
        ImGui::SetNextItemWidth(64);
        ImGui::InputInt(bucketLabels[i], &s.params.betCurve[i], 0, 0);
        s.params.betCurve[i] = std::max(1, s.params.betCurve[i]);
      }
      ImGui::TreePop();
    }
  }

  ImGui::Checkbox("Allow debt (negative bank)", &s.params.debtAllowed);
  ImGui::SetItemTooltip("When enabled, play continues even if the bankroll goes negative.\n"
                        "When disabled, the simulation stops when the bank is exhausted.");

  ImGui::PopItemWidth();
  ImGui::EndDisabled();
}

void drawRunsContent(AppState &s) {
  // Run controls sit at the top of this section.
  ImGui::BeginDisabled(s.running);
  ImGui::PushStyleColor(ImGuiCol_Button, pal::accent);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, pal::accentBright);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, pal::accentDim);
  ImGui::PushStyleColor(ImGuiCol_Text, rgb(16, 19, 25));
  ImGui::PushFont(gFontHead);
  if (ImGui::Button("RUN", ImVec2(-FLT_MIN, 0)))
    startRun(s);
  ImGui::PopFont();
  ImGui::PopStyleColor(4);
  ImGui::EndDisabled();

  ImGui::BeginDisabled(!s.running);
  if (ImGui::Button("Stop", ImVec2(-FLT_MIN, 0)) && s.monitor) {
    s.monitor->stopRequested.store(true, std::memory_order_relaxed);
    s.wasStopped = true;
  }
  ImGui::EndDisabled();

  ImGui::Spacing();
  {
    // Status line with a small state LED.
    const ImVec4 led = s.running ? pal::accent
                       : s.haveResult
                           ? (s.wasStopped ? pal::neg : pal::pos)
                           : pal::textDim;
    const char *label = s.running ? "RUNNING"
                        : s.haveResult ? (s.wasStopped ? "STOPPED EARLY"
                                                       : "COMPLETED")
                                       : "IDLE";
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float cy = p.y + ImGui::GetTextLineHeight() * 0.5f;
    ImGui::GetWindowDrawList()->AddCircleFilled(
        ImVec2(p.x + 4.0f, cy), 4.0f, ImGui::ColorConvertFloat4ToU32(led));
    ImGui::Dummy(ImVec2(14.0f, 0.0f));
    ImGui::SameLine();
    ImGui::PushFont(gFontHead);
    ImGui::TextColored(led, "%s", label);
    ImGui::PopFont();
  }
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  int removeIdx = -1;
  for (size_t r = 0; r < s.history.size(); ++r) {
    RunRecord &rec = s.history[r];
    ImGui::PushID(rec.id);
    ImGui::Checkbox("##vis", &rec.visible);
    ImGui::SameLine();
    ImGui::ColorEdit3("##color", &rec.color.x,
                      ImGuiColorEditFlags_NoInputs |
                          ImGuiColorEditFlags_NoLabel);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::InputText("##name", &rec.label);
    if (ImGui::IsItemHovered() && !rec.desc.empty())
      ImGui::SetTooltip("%s", rec.desc.c_str());
    ImGui::SameLine();
    ImGui::Checkbox("avg", &rec.showAvg);
    ImGui::SameLine();
    if (ImGui::SmallButton("x"))
      removeIdx = static_cast<int>(r);
    ImGui::PopID();
  }
  if (removeIdx >= 0)
    s.history.erase(s.history.begin() + removeIdx);
  if (!s.history.empty() &&
      ImGui::Button("Clear runs", ImVec2(-FLT_MIN, 0)))
    s.history.clear();

  ImGui::BeginDisabled(s.history.empty());
  if (ImGui::Button("Export runs", ImVec2(-FLT_MIN, 0))) {
    const std::string dest =
        pfd::save_file("Export runs", s.ioPath,
                       {"JSON files (*.json)", "*.json", "All files", "*"})
            .result();
    if (!dest.empty()) {
      s.ioPath = dest;
      s.ioStatus = exportRuns(s);
    }
  }
  ImGui::EndDisabled();
  if (ImGui::Button("Import runs", ImVec2(-FLT_MIN, 0))) {
    const std::vector<std::string> sel =
        pfd::open_file("Import runs", s.ioPath,
                       {"JSON files (*.json)", "*.json", "All files", "*"})
            .result();
    if (!sel.empty()) {
      s.ioPath = sel.front();
      s.ioStatus = importRuns(s);
    }
  }
  if (!s.ioStatus.empty())
    ImGui::TextWrapped("%s", s.ioStatus.c_str());
}

// Left column: PARAMETERS and RUNS as two collapsible sections whose shared
// border can be dragged to re-balance their heights.
void drawLeftColumn(AppState &s, float width) {
  ImGui::BeginChild("left", ImVec2(width, 0), false);
  const ImGuiStyle &style = ImGui::GetStyle();
  const float headerReserve = ImGui::GetFrameHeight() + style.ItemSpacing.y;
  const float splitterH = 8.0f;
  const float minBody = 64.0f;

  ImGui::PushFont(gFontHead);
  const bool paramsOpen =
      ImGui::CollapsingHeader("PARAMETERS", ImGuiTreeNodeFlags_DefaultOpen);
  ImGui::PopFont();

  // Space available for the two bodies once the run header (and, when both are
  // open, the splitter) is accounted for.
  const bool bothOpen = paramsOpen && s.runsOpen;
  float bodyRegion = ImGui::GetContentRegionAvail().y - headerReserve;
  if (bothOpen)
    bodyRegion -= splitterH + style.ItemSpacing.y;
  bodyRegion = std::max(bodyRegion, minBody * 2.0f);

  if (paramsOpen) {
    const float h = bothOpen ? std::min(std::max(s.paramsPanelH, minBody),
                                        bodyRegion - minBody)
                             : bodyRegion;
    ImGui::BeginChild("paramsBody", ImVec2(0, h), ImGuiChildFlags_Border);
    drawParamsContent(s);
    ImGui::EndChild();
  }

  if (bothOpen) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, pal::accentSoft);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, pal::accentSoft);
    ImGui::Button("##split", ImVec2(-1.0f, splitterH));
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemActive())
      s.paramsPanelH += ImGui::GetIO().MouseDelta.y;
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
      ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    // Faint centre grip so the divider reads as draggable.
    const ImVec2 mn = ImGui::GetItemRectMin();
    const ImVec2 mx = ImGui::GetItemRectMax();
    const float cy = (mn.y + mx.y) * 0.5f;
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(mn.x + 10.0f, cy), ImVec2(mx.x - 10.0f, cy),
        ImGui::ColorConvertFloat4ToU32(pal::border), 1.0f);
    s.paramsPanelH = std::min(std::max(s.paramsPanelH, minBody),
                              bodyRegion - minBody);
  }

  ImGui::PushFont(gFontHead);
  const bool runsOpen =
      ImGui::CollapsingHeader("RUNS", ImGuiTreeNodeFlags_DefaultOpen);
  ImGui::PopFont();
  if (runsOpen) {
    ImGui::BeginChild("runsBody", ImVec2(0, 0), ImGuiChildFlags_Border);
    drawRunsContent(s);
    ImGui::EndChild();
  }

  s.paramsOpen = paramsOpen;
  s.runsOpen = runsOpen;
  ImGui::EndChild();
}

int metricFormatter(double value, char *buff, int size, void *) {
  static const double thresholds[] = {1e12, 1e9, 1e6, 1e3};
  static const char *suffixes[] = {"T", "B", "M", "K"};
  const double v = value < 0 ? -value : value;
  for (int i = 0; i < 4; ++i) {
    if (v >= thresholds[i])
      return std::snprintf(buff, size, "%g%s", value / thresholds[i],
                           suffixes[i]);
  }
  return std::snprintf(buff, size, "%g", value);
}

void plotSeries(const char *label, const std::vector<double> &x,
                const std::vector<double> &y, double startBank,
                bool normalize) {
  if (x.empty())
    return;
  std::vector<double> adj(y.size());
  if (normalize) {
    for (size_t i = 0; i < y.size(); ++i)
      adj[i] = (y[i] - startBank) / startBank * 100.0;
  } else {
    for (size_t i = 0; i < y.size(); ++i)
      adj[i] = y[i] - startBank;
  }
  ImPlot::PlotLine(label, x.data(), adj.data(), static_cast<int>(x.size()));
}

void drawPlot(AppState &s, float height) {
  ImGui::Checkbox("Normalize (% of starting bank)", &s.normalize);
  ImGui::SameLine();
  if (ImGui::Button("Export PNG")) {
    const std::string dest =
        pfd::save_file("Export plot", s.pngPath,
                       {"PNG image (*.png)", "*.png", "All files", "*"})
            .result();
    if (!dest.empty()) {
      s.pngPath = dest;
      s.wantPlotExport = true;
    }
  }
  if (ImPlot::BeginPlot("Bank balance", ImVec2(-1, height))) {
    ImPlot::SetupAxes("hands played",
                      s.normalize ? "profit / loss (%)" : "profit / loss",
                      ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
    ImPlot::SetupAxisFormat(ImAxis_X1, metricFormatter);
    if (!s.normalize)
      ImPlot::SetupAxisFormat(ImAxis_Y1, metricFormatter);

    for (size_t r = 0; r < s.history.size(); ++r) {
      RunRecord &rec = s.history[r];
      if (rec.visible) {
        // Individual threads render as a dimmed variant of the run colour;
        // the average (below) uses the full-strength colour so it reads on top.
        const ImVec4 threadColor = shade(rec.color, 0.55f);
        for (size_t t = 0; t < rec.xs.size(); ++t) {
          ImPlot::SetNextLineStyle(threadColor);
          plotSeries(rec.label.c_str(), rec.xs[t], rec.ys[t], rec.startBank,
                     s.normalize);
        }
      }
      if (rec.showAvg) {
        ImPlot::SetNextLineStyle(rec.color, 3.0f);
        plotSeries((rec.label + " avg").c_str(), rec.avgX, rec.avgY,
                   rec.startBank, s.normalize);
      }
    }

    if (s.running) {
      for (size_t t = 0; t < s.xs.size(); ++t) {
        char label[32];
        std::snprintf(label, sizeof(label), "thread %d", static_cast<int>(t));
        plotSeries(label, s.xs[t], s.ys[t], s.runParams.bank, s.normalize);
      }
    }
    ImPlot::EndPlot();
  }
  s.plotMin = ImGui::GetItemRectMin();
  s.plotMax = ImGui::GetItemRectMax();
}

// 26 label/value pairs, laid out 3 pairs per table row.
enum { kStatPairs = 26, kStatPairsPerRow = 3 };

float statsPanelHeight(const AppState &s) {
  const ImGuiStyle &style = ImGui::GetStyle();
  const int rows =
      (kStatPairs + kStatPairsPerRow - 1) / kStatPairsPerRow;
  float h = ImGui::GetFrameHeight() + style.ItemSpacing.y; // SeparatorText
  h += rows * (ImGui::GetTextLineHeight() + 2.0f * style.CellPadding.y);
  h += 2.0f * style.WindowPadding.y + style.ItemSpacing.y + 8.0f;
  return h;
}

void drawStats(AppState &s, float height) {
  const Stats &st = s.live;
  const int threads = std::max(1, s.runParams.threads);
  const int64_t startingTotal =
      static_cast<int64_t>(s.runParams.bank) * threads;
  const int64_t profit = st.bank - startingTotal;

  const double elapsed =
      s.running
          ? std::chrono::duration<double>(Clock::now() - s.startTime).count()
          : (s.haveResult ? std::chrono::duration<double>(s.endTime -
                                                          s.startTime)
                                .count()
                          : 0.0);

  double worstDrawdown = s.finalDrawdown;
  if (s.running) {
    worstDrawdown = 0.0;
    for (size_t t = 0; t < s.ys.size(); ++t)
      worstDrawdown = std::max(worstDrawdown, maxDrawdown(s.ys[t]));
  }

  ImGui::BeginChild("stats", ImVec2(0, height), ImGuiChildFlags_Border);
  sectionHeader(s.running ? "LIVE" : "RESULTS");

  if (ImGui::BeginTable("statstable", kStatPairsPerRow * 2,
                        ImGuiTableFlags_SizingStretchProp)) {
    // color == nullptr renders in the default text colour; profit-derived
    // figures carry a green/red tint by sign, drawdown always reads as loss.
    struct Item {
      const char *label;
      std::string value;
      const ImVec4 *color;
      Item(const char *l, std::string v, const ImVec4 *c = nullptr)
          : label(l), value(std::move(v)), color(c) {}
    };
    const ImVec4 *profitCol = profit >= 0 ? &pal::pos : &pal::neg;
    std::vector<Item> items;
    items.push_back({"Hands played", fmtInt(st.hands)});
    items.push_back({"Total bet", fmtInt(st.totalBet)});
    items.push_back({"Player wins", fmtInt(st.playerWins)});
    items.push_back({"Win rate", fmtPct(divide(st.playerWins, st.hands))});
    items.push_back({"Dealer wins", fmtInt(st.dealerWins)});
    items.push_back({"Loss rate", fmtPct(divide(st.dealerWins, st.hands))});
    items.push_back({"Draws", fmtInt(st.draw)});
    items.push_back({"Draw rate", fmtPct(divide(st.draw, st.hands))});
    items.push_back({"Player blackjacks", fmtInt(st.playerBlackjacks)});
    items.push_back(
        {"Player BJ rate", fmtPct(divide(st.playerBlackjacks, st.hands))});
    items.push_back({"Dealer blackjacks", fmtInt(st.dealerBlackjacks)});
    items.push_back(
        {"Dealer BJ rate", fmtPct(divide(st.dealerBlackjacks, st.hands))});
    items.push_back({"Splits", fmtInt(st.splits)});
    items.push_back({"Doubles", fmtInt(st.doubles)});
    items.push_back({"Shuffles", fmtInt(st.shuffles)});
    items.push_back({"Cards dealt", fmtInt(st.cardsDealt)});
    items.push_back({"Total bank", fmtInt(st.bank)});
    items.push_back(
        {"Avg player bank", fmtDouble(divide(st.bank, threads))});
    items.push_back({"Total profit", fmtInt(profit), profitCol});
    items.push_back(
        {"Avg profit", fmtDouble(divide(profit, threads)), profitCol});
    items.push_back(
        {"EV per hand", fmtDouble(divide(profit, st.hands), " $"), profitCol});
    items.push_back(
        {"EV percentage", fmtPct(divide(profit, st.totalBet)), profitCol});
    items.push_back(
        {"Avg bet", fmtDouble(divide(st.totalBet, st.hands))});
    items.push_back({"Worst drawdown", fmtDouble(worstDrawdown), &pal::neg});
    items.push_back({"Elapsed", fmtDouble(elapsed, " s")});
    items.push_back(
        {"Hands per second",
         fmtInt(static_cast<int64_t>(
             elapsed > 0.0 ? static_cast<double>(st.hands) / elapsed : 0.0))});

    // Keep kStatPairs in sync with the list above.
    for (size_t i = 0; i < items.size(); i += kStatPairsPerRow) {
      ImGui::TableNextRow();
      for (size_t p = 0; p < kStatPairsPerRow && i + p < items.size(); ++p) {
        const Item &it = items[i + p];
        ImGui::TableSetColumnIndex(static_cast<int>(p * 2));
        ImGui::TextDisabled("%s", it.label);
        ImGui::TableSetColumnIndex(static_cast<int>(p * 2 + 1));
        ImGui::PushFont(gFontMono);
        if (it.color)
          ImGui::TextColored(*it.color, "%s", it.value.c_str());
        else
          ImGui::TextUnformatted(it.value.c_str());
        ImGui::PopFont();
      }
    }
    ImGui::EndTable();
  }
  ImGui::EndChild();
}

void drawFrame(AppState &s, int displayW, int displayH) {
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2(static_cast<float>(displayW),
                                  static_cast<float>(displayH)));
  ImGui::Begin("Blackjack Simulator", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                   ImGuiWindowFlags_NoSavedSettings);

  drawLeftColumn(s, 280.0f);
  ImGui::SameLine();

  ImGui::BeginGroup();
  const float statsHeight = statsPanelHeight(s);
  const float plotHeight =
      std::max(140.0f, ImGui::GetContentRegionAvail().y - statsHeight -
                           ImGui::GetStyle().ItemSpacing.y -
                           ImGui::GetFrameHeightWithSpacing());
  drawPlot(s, plotHeight);
  drawStats(s, statsHeight);
  ImGui::EndGroup();

  ImGui::End();
}

void glfwErrorCallback(int error, const char *description) {
  std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

// Generate RGBA pixel data for a simple diamond-shaped icon at the given size.
// Uses the app's accent colour on a dark background — no external file needed.
std::vector<unsigned char> makeIconPixels(int size) {
  std::vector<unsigned char> pixels(static_cast<size_t>(size * size * 4));
  const float cx = (size - 1) * 0.5f;
  const float cy = (size - 1) * 0.5f;
  // Outer diamond fills ~76 % of the tile; inner cutout adds a border effect.
  const float outer = size * 0.38f;
  const float inner = size * 0.22f;
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const float dx = std::abs(static_cast<float>(x) - cx);
      const float dy = std::abs(static_cast<float>(y) - cy);
      const float dist = dx + dy;
      unsigned char *p = &pixels[static_cast<size_t>((y * size + x) * 4)];
      if (dist <= outer && dist > inner) {
        // Accent ring — muted slate blue
        p[0] = 124; p[1] = 146; p[2] = 173; p[3] = 255;
      } else if (dist <= inner) {
        // Centre fill — slightly brighter
        p[0] = 158; p[1] = 178; p[2] = 202; p[3] = 255;
      } else {
        // Background — dark panel colour
        p[0] = 14; p[1] = 17; p[2] = 22; p[3] = 255;
      }
    }
  }
  return pixels;
}

// ---------------------------------------------------------------------------
// Visual theme: a modern, clean dark look that steps away from the default
// ImGui palette. Deep neutral panels, soft rounded corners, generous spacing,
// and a single teal accent used consistently across interactive widgets.
// ---------------------------------------------------------------------------

void setupTheme() {
  ImGuiStyle &style = ImGui::GetStyle();

  // Shape: soft rounding and breathing room.
  style.WindowRounding = 8.0f;
  style.ChildRounding = 8.0f;
  style.FrameRounding = 6.0f;
  style.PopupRounding = 6.0f;
  style.GrabRounding = 6.0f;
  style.TabRounding = 6.0f;
  style.ScrollbarRounding = 8.0f;

  style.WindowBorderSize = 0.0f;
  style.ChildBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.PopupBorderSize = 1.0f;

  style.WindowPadding = ImVec2(10, 10);
  style.FramePadding = ImVec2(8, 4);
  style.CellPadding = ImVec2(6, 3);
  style.ItemSpacing = ImVec2(8, 5);
  style.ItemInnerSpacing = ImVec2(6, 4);
  style.ScrollbarSize = 11.0f;
  style.GrabMinSize = 11.0f;
  style.SeparatorTextBorderSize = 2.0f;
  style.SeparatorTextPadding = ImVec2(14, 4);

  // Palette (shared with the stats readout via pal::).
  const ImVec4 accent = pal::accent;
  const ImVec4 accentDim = pal::accentDim;
  const ImVec4 accentSoft = pal::accentSoft;
  const ImVec4 text = pal::text;
  const ImVec4 textDim = pal::textDim;
  const ImVec4 bgWindow = pal::bgWindow;
  const ImVec4 bgChild = pal::bgChild;
  const ImVec4 bgFrame = pal::bgFrame;
  const ImVec4 bgFrameHover = pal::bgFrameHover;
  const ImVec4 bgFrameActive = pal::bgFrameActive;
  const ImVec4 border = pal::border;

  ImVec4 *c = style.Colors;
  c[ImGuiCol_Text] = text;
  c[ImGuiCol_TextDisabled] = textDim;
  c[ImGuiCol_WindowBg] = bgWindow;
  c[ImGuiCol_ChildBg] = bgChild;
  c[ImGuiCol_PopupBg] = rgb(18, 22, 30, 0.98f);
  c[ImGuiCol_Border] = border;
  c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_FrameBg] = bgFrame;
  c[ImGuiCol_FrameBgHovered] = bgFrameHover;
  c[ImGuiCol_FrameBgActive] = bgFrameActive;
  c[ImGuiCol_TitleBg] = bgWindow;
  c[ImGuiCol_TitleBgActive] = bgWindow;
  c[ImGuiCol_TitleBgCollapsed] = bgWindow;
  c[ImGuiCol_MenuBarBg] = bgChild;
  c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_ScrollbarGrab] = rgb(60, 66, 78);
  c[ImGuiCol_ScrollbarGrabHovered] = rgb(76, 84, 98);
  c[ImGuiCol_ScrollbarGrabActive] = accentDim;
  c[ImGuiCol_CheckMark] = accent;
  c[ImGuiCol_SliderGrab] = accent;
  c[ImGuiCol_SliderGrabActive] = accentDim;
  c[ImGuiCol_Button] = bgFrame;
  c[ImGuiCol_ButtonHovered] = bgFrameHover;
  c[ImGuiCol_ButtonActive] = accentDim;
  c[ImGuiCol_Header] = accentSoft;
  c[ImGuiCol_HeaderHovered] = rgb(124, 146, 173, 0.28f);
  c[ImGuiCol_HeaderActive] = rgb(124, 146, 173, 0.38f);
  c[ImGuiCol_Separator] = border;
  c[ImGuiCol_SeparatorHovered] = accentSoft;
  c[ImGuiCol_SeparatorActive] = accent;
  c[ImGuiCol_ResizeGrip] = rgb(60, 66, 78);
  c[ImGuiCol_ResizeGripHovered] = accentSoft;
  c[ImGuiCol_ResizeGripActive] = accent;
  c[ImGuiCol_Tab] = bgFrame;
  c[ImGuiCol_TabHovered] = accentSoft;
  c[ImGuiCol_TabActive] = accentDim;
  c[ImGuiCol_TableHeaderBg] = bgFrame;
  c[ImGuiCol_TableBorderStrong] = border;
  c[ImGuiCol_TableBorderLight] = rgb(255, 255, 255, 0.03f);
  c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_TableRowBgAlt] = rgb(255, 255, 255, 0.02f);
  c[ImGuiCol_TextSelectedBg] = accentSoft;
  c[ImGuiCol_NavHighlight] = accent;
  c[ImGuiCol_PlotLines] = accent;
  c[ImGuiCol_PlotHistogram] = accent;
}

void setupPlotTheme() {
  ImPlotStyle &ps = ImPlot::GetStyle();
  ps.LineWeight = 1.4f;
  ps.PlotPadding = ImVec2(12, 12);
  ps.LabelPadding = ImVec2(6, 6);
  ps.PlotBorderSize = 0.0f;
  ps.MinorAlpha = 0.20f;

  ImVec4 *pc = ps.Colors;
  pc[ImPlotCol_FrameBg] = ImVec4(0, 0, 0, 0);
  pc[ImPlotCol_PlotBg] = rgb(9, 12, 17); // slightly recessed from the panel
  pc[ImPlotCol_PlotBorder] = ImVec4(0, 0, 0, 0);
  pc[ImPlotCol_AxisGrid] = rgb(255, 255, 255, 0.06f);
  pc[ImPlotCol_AxisText] = pal::textDim;
  pc[ImPlotCol_TitleText] = pal::text;
  pc[ImPlotCol_LegendBg] = rgb(16, 20, 28, 0.94f);
  pc[ImPlotCol_LegendBorder] = pal::border;
  pc[ImPlotCol_LegendText] = pal::text;

  // A clean, well-separated colormap for overlaid runs.
  ps.Colormap = ImPlotColormap_Deep;
}

void loadFonts() {
  ImGuiIO &io = ImGui::GetIO();
#ifdef BJ_ASSETS_DIR
  ImFontConfig cfg;
  cfg.OversampleH = 3;
  cfg.OversampleV = 2;
  cfg.PixelSnapH = false;
  // First font added becomes the default UI face.
  gFontUI = io.Fonts->AddFontFromFileTTF(
      BJ_ASSETS_DIR "/fonts/IBMPlexSans-Regular.ttf", 14.0f, &cfg);
  gFontHead = io.Fonts->AddFontFromFileTTF(
      BJ_ASSETS_DIR "/fonts/IBMPlexSans-SemiBold.ttf", 13.0f, &cfg);
  gFontMono = io.Fonts->AddFontFromFileTTF(
      BJ_ASSETS_DIR "/fonts/IBMPlexMono-Medium.ttf", 13.0f, &cfg);
#endif
  // Fall back to the built-in bitmap font if any face failed to load.
  if (gFontUI == nullptr)
    gFontUI = io.Fonts->AddFontDefault();
  if (gFontHead == nullptr)
    gFontHead = gFontUI;
  if (gFontMono == nullptr)
    gFontMono = gFontUI;
}

} // namespace

int main(int, char **) {
  glfwSetErrorCallback(glfwErrorCallback);
  if (!glfwInit())
    return 1;

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  GLFWwindow *window =
      glfwCreateWindow(1280, 800, "Blackjack Simulator", nullptr, nullptr);
  if (!window) {
    glfwTerminate();
    return 1;
  }
  {
    // Set the window / taskbar icon.
    auto px32 = makeIconPixels(32);
    auto px16 = makeIconPixels(16);
    GLFWimage icons[2];
    icons[0] = {32, 32, px32.data()};
    icons[1] = {16, 16, px16.data()};
    glfwSetWindowIcon(window, 2, icons);
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  ImGui::StyleColorsDark();
  setupTheme();
  setupPlotTheme();
  loadFonts();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 130");

  AppState state;

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    pollSim(state);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    int displayW, displayH;
    glfwGetFramebufferSize(window, &displayW, &displayH);
    drawFrame(state, displayW, displayH);

    ImGui::Render();
    glViewport(0, 0, displayW, displayH);
    glClearColor(0.039f, 0.047f, 0.063f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (state.wantPlotExport) {
      state.wantPlotExport = false;
      const ImGuiIO &io = ImGui::GetIO();
      const float sx = static_cast<float>(displayW) / io.DisplaySize.x;
      const float sy = static_cast<float>(displayH) / io.DisplaySize.y;
      const int px = static_cast<int>(state.plotMin.x * sx);
      const int py = static_cast<int>(state.plotMin.y * sy);
      const int pw = static_cast<int>((state.plotMax.x - state.plotMin.x) * sx);
      const int ph = static_cast<int>((state.plotMax.y - state.plotMin.y) * sy);
      if (pw > 0 && ph > 0) {
        std::vector<unsigned char> pixels(
            static_cast<size_t>(pw) * ph * 4);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(px, displayH - py - ph, pw, ph, GL_RGBA,
                     GL_UNSIGNED_BYTE, pixels.data());
        for (size_t i = 3; i < pixels.size(); i += 4)
          pixels[i] = 255;
        stbi_flip_vertically_on_write(1);
        state.ioStatus =
            stbi_write_png(state.pngPath.c_str(), pw, ph, 4, pixels.data(),
                           pw * 4)
                ? "saved plot to " + state.pngPath
                : "failed to write " + state.pngPath;
      }
    }

    glfwSwapBuffers(window);
  }

  // If a run is still active, request stop and wait for it.
  if (state.running && state.monitor) {
    state.monitor->stopRequested.store(true);
    if (state.future.valid())
      state.future.wait();
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImPlot::DestroyContext();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
