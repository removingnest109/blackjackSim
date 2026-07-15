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
#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct GuiParams {
  int hands = 10000000;
  int decks = 6;
  int bank = 100000;
  int bet = 10;
  float penetration = 0.75f;
  int threads = 1;
  bool dealerHitSoft17 = false;
  bool cardCounting = false;
  bool debtAllowed = false;
};

struct AppState {
  GuiParams params;

  bool running = false;
  bool haveResult = false;
  bool wasStopped = false;

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

void startRun(AppState &s) {
  config.numberHands = s.params.hands;
  config.numberDecks = s.params.decks;
  config.startingBank = s.params.bank;
  config.defaultBetSize = s.params.bet;
  config.penetrationBeforeShuffle = s.params.penetration;
  config.dealerHitSoft17 = s.params.dealerHitSoft17;
  config.cardCounting = s.params.cardCounting;
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

void drawParamsPanel(AppState &s) {
  ImGui::BeginChild("params", ImVec2(280, 0), ImGuiChildFlags_Border);
  ImGui::SeparatorText("Parameters");

  ImGui::BeginDisabled(s.running);
  ImGui::PushItemWidth(-FLT_MIN);

  ImGui::TextDisabled("Hands per thread");
  ImGui::InputInt("##hands", &s.params.hands, 0, 0);
  s.params.hands = std::max(1, s.params.hands);

  ImGui::TextDisabled("Decks");
  ImGui::SliderInt("##decks", &s.params.decks, 1, 12);

  ImGui::TextDisabled("Starting bank");
  ImGui::InputInt("##bank", &s.params.bank, 0, 0);
  s.params.bank = std::max(1, s.params.bank);

  ImGui::TextDisabled("Bet size");
  ImGui::InputInt("##bet", &s.params.bet, 0, 0);
  s.params.bet = std::max(1, s.params.bet);

  ImGui::TextDisabled("Shuffle penetration");
  ImGui::SliderFloat("##pen", &s.params.penetration, 0.05f, 1.0f, "%.2f");

  const int maxThreads =
      std::max(1u, std::thread::hardware_concurrency());
  ImGui::TextDisabled("Threads");
  ImGui::SliderInt("##threads", &s.params.threads, 1, maxThreads);

  ImGui::Spacing();
  ImGui::Checkbox("Dealer hits soft 17", &s.params.dealerHitSoft17);
  ImGui::Checkbox("Card counting", &s.params.cardCounting);
  ImGui::Checkbox("Allow debt (negative bank)", &s.params.debtAllowed);

  ImGui::PopItemWidth();
  ImGui::Spacing();

  if (ImGui::Button("Run", ImVec2(-FLT_MIN, 0)))
    startRun(s);
  ImGui::EndDisabled();

  ImGui::BeginDisabled(!s.running);
  if (ImGui::Button("Stop", ImVec2(-FLT_MIN, 0)) && s.monitor) {
    s.monitor->stopRequested.store(true, std::memory_order_relaxed);
    s.wasStopped = true;
  }
  ImGui::EndDisabled();

  ImGui::Spacing();
  if (s.running)
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.4f, 1.0f), "Running...");
  else if (s.haveResult)
    ImGui::TextDisabled(s.wasStopped ? "Stopped early" : "Completed");
  else
    ImGui::TextDisabled("Idle");

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

void drawPlot(AppState &s, float height) {
  if (ImPlot::BeginPlot("Bank balance per thread", ImVec2(-1, height))) {
    ImPlot::SetupAxes("hands played", "bank",
                      ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
    ImPlot::SetupAxisFormat(ImAxis_X1, metricFormatter);
    ImPlot::SetupAxisFormat(ImAxis_Y1, metricFormatter);
    for (size_t t = 0; t < s.xs.size(); ++t) {
      if (s.xs[t].empty())
        continue;
      char label[32];
      std::snprintf(label, sizeof(label), "thread %d", static_cast<int>(t));
      ImPlot::PlotLine(label, s.xs[t].data(), s.ys[t].data(),
                       static_cast<int>(s.xs[t].size()));
    }
    ImPlot::EndPlot();
  }
}

void drawStats(AppState &s) {
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

  double worstDrawdown = 0.0;
  for (size_t t = 0; t < s.ys.size(); ++t)
    worstDrawdown = std::max(worstDrawdown, maxDrawdown(s.ys[t]));

  ImGui::BeginChild("stats", ImVec2(0, 0), ImGuiChildFlags_Border);
  ImGui::SeparatorText(s.running ? "Live statistics" : "Results");

  if (ImGui::BeginTable("statstable", 4,
                        ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("l0");
    ImGui::TableSetupColumn("v0");
    ImGui::TableSetupColumn("l1");
    ImGui::TableSetupColumn("v1");

    // Row-pair helper: two label/value pairs per table row.
    struct Item {
      const char *label;
      std::string value;
    };
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
    items.push_back({"Total profit", fmtInt(profit)});
    items.push_back({"Avg profit", fmtDouble(divide(profit, threads))});
    items.push_back(
        {"EV per hand", fmtDouble(divide(profit, st.hands), " $")});
    items.push_back({"EV percentage", fmtPct(divide(profit, st.totalBet))});
    items.push_back(
        {"Avg bet", fmtDouble(divide(st.totalBet, st.hands))});
    items.push_back({"Worst drawdown", fmtDouble(worstDrawdown)});
    items.push_back({"Elapsed", fmtDouble(elapsed, " s")});
    items.push_back(
        {"Hands per second",
         fmtInt(static_cast<int64_t>(
             elapsed > 0.0 ? static_cast<double>(st.hands) / elapsed : 0.0))});

    for (size_t i = 0; i + 1 < items.size(); i += 2) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextDisabled("%s", items[i].label);
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(items[i].value.c_str());
      ImGui::TableSetColumnIndex(2);
      ImGui::TextDisabled("%s", items[i + 1].label);
      ImGui::TableSetColumnIndex(3);
      ImGui::TextUnformatted(items[i + 1].value.c_str());
    }
    ImGui::EndTable();
  }

  if (s.runParams.cardCounting && s.monitor) {
    ImGui::SeparatorText("Card counting (per thread)");
    for (size_t t = 0; t < s.monitor->probes.size(); ++t) {
      const Stats snap = s.monitor->probes[t]->readLatest();
      ImGui::TextDisabled("thread %d", static_cast<int>(t));
      ImGui::SameLine();
      ImGui::Text("running count %s, true count %.2f, cards since shuffle %s",
                  fmtInt(snap.runningCount).c_str(), snap.trueCount,
                  fmtInt(snap.cardsSinceShuffle).c_str());
    }
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

  drawParamsPanel(s);
  ImGui::SameLine();

  ImGui::BeginGroup();
  const float statsHeight = 240.0f;
  const float plotHeight =
      std::max(160.0f, ImGui::GetContentRegionAvail().y - statsHeight -
                           ImGui::GetStyle().ItemSpacing.y);
  drawPlot(s, plotHeight);
  drawStats(s);
  ImGui::EndGroup();

  ImGui::End();
}

void glfwErrorCallback(int error, const char *description) {
  std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
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
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  ImGui::StyleColorsDark();
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
    glClearColor(0.06f, 0.06f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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
