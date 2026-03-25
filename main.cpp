#include <thread>
#include "cli.h"
#include "config.h"
#include "stats.h"
#include "print.h"
#include "simulation.h"

int main(const int argc, char **argv) {
  getArgs(argc, argv);
  if (!config.isInteractive && config.multiThread)
    config.threads = std::thread::hardware_concurrency();
  if (config.threads == 0)
    config.threads = 1;
  if (config.verbose)
    printGlobalVars(config);
  printStats(runSim(), config);
  return 0;
}
