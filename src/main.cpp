#include "cli.h"
#include "config.h"
#include "monitor.h"
#include "print.h"
#include "runjson.h"
#include "simulation.h"
#include "stats.h"
#include <chrono>
#include <iostream>
#include <thread>

int main(const int argc, char **argv) {
  getArgs(argc, argv);
  if (config.multiThread)
    config.threads = std::thread::hardware_concurrency();
  if (config.threads == 0)
    config.threads = 1;
  if (config.verbose)
    printGlobalVars();

  if (!config.saveJsonPath.empty()) {
    // Capture per-thread bank timelines the same way the GUI does, then write
    // the run as JSON instead of printing stats.
    SimMonitor monitor(config.threads, std::max(1, config.playersPerTable),
                       config.startingBank);
    const auto start = std::chrono::steady_clock::now();
    const Stats result = runSim(&monitor);
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    if (saveRunJson(config.saveJsonPath, monitor, result, elapsed))
      std::cout << "saved run to " << config.saveJsonPath << "\n";
    else
      std::cerr << "failed to write " << config.saveJsonPath << "\n";
    return 0;
  }

  printStats(runSim());
  return 0;
}
