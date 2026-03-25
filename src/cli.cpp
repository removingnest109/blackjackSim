#include "cli.h"
#include "config.h"
#include <iostream>

void printHelp() {
  std::cout
      << "Options:\n"
         "  -h, --help                     Show help\n"
         "  -v, --verbose                  Enable verbose mode\n"
         "  -n, --hands <num>              Number of hands (default 10000000)\n"
         "  -d, --decks <num>              Number of decks (default 6)\n"
         "  -b, --bank <amount>            Starting bank (default 100000)\n"
         "  -t, --bet <amount>             Default bet size (default 10)\n"
         "  -p, --penetration <0.0-1.0>    Shuffle penetration (default 0.75)\n"
         "  -s, --dealer-hit-soft-17       Dealer hits soft 17\n"
         "  -i, --interactive              Interactive mode\n"
         "  -c, --card-counting            Enable card counting\n"
         "  -e, --debt                     Enable negative bank\n"
         "  -m, --multithread              Enable multithreading\n";
}

bool requiresValue(const std::string &arg) {
  return arg == "-n" || arg == "--hands" || arg == "-d" || arg == "--decks" ||
         arg == "-b" || arg == "--bank" || arg == "-t" || arg == "--bet" ||
         arg == "-p" || arg == "--penetration";
}

void getArgs(const int argc, char **argv) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];

    if (arg.rfind("--", 0) == 0) { // starts with --
      if (arg == "--help") {
        printHelp();
        std::exit(0);
      } else if (arg == "--verbose")
        config.verbose = true;
      else if (arg == "--dealer-hit-soft-17")
        config.dealerHitSoft17 = true;
      else if (arg == "--interactive")
        config.isInteractive = true;
      else if (arg == "--card-counting")
        config.cardCounting = true;
      else if (arg == "--debt")
        config.debtAllowed = true;
      else if (arg == "--multithread")
        config.multiThread = true;

      else if (arg == "--hands" || arg == "--decks" || arg == "--bank" ||
               arg == "--bet" || arg == "--penetration") {
        if (i + 1 >= argc) {
          std::cerr << "Missing value for " << arg << "\n";
          std::exit(1);
        }
        std::string value = argv[++i];
        try {
          if (arg == "--hands")
            config.numberHands = std::stoi(value);
          else if (arg == "--decks")
            config.numberDecks = std::stoi(value);
          else if (arg == "--bank")
            config.startingBank = std::stoi(value);
          else if (arg == "--bet")
            config.defaultBetSize = std::stoi(value);
          else if (arg == "--penetration")
            config.penetrationBeforeShuffle = std::stof(value);
        } catch (...) {
          std::cerr << "Invalid value for " << arg << "\n";
          std::exit(1);
        }
      } else {
        std::cerr << "Unknown argument: " << arg << "\n";
        std::exit(1);
      }
    }

    else if (arg[0] == '-' && arg.size() > 1) {
      for (size_t j = 1; j < arg.size(); ++j) {
        switch (const char flag = arg[j]) {
        case 'h':
          printHelp();
          std::exit(0);
        case 'v':
          config.verbose = true;
          break;
        case 's':
          config.dealerHitSoft17 = true;
          break;
        case 'i':
          config.isInteractive = true;
          break;
        case 'c':
          config.cardCounting = true;
          break;
        case 'e':
          config.debtAllowed = true;
          break;
        case 'm':
          config.multiThread = true;
          break;

        case 'n':
        case 'd':
        case 'b':
        case 't':
        case 'p': {
          if (j + 1 != arg.size()) {
            std::cerr << "Option -" << flag << " requires a separate value\n";
            std::exit(1);
          }
          if (i + 1 >= argc) {
            std::cerr << "Missing value for -" << flag << "\n";
            std::exit(1);
          }
          std::string value = argv[++i];
          try {
            if (flag == 'n')
              config.numberHands = std::stoi(value);
            else if (flag == 'd')
              config.numberDecks = std::stoi(value);
            else if (flag == 'b')
              config.startingBank = std::stoi(value);
            else if (flag == 't')
              config.defaultBetSize = std::stoi(value);
            else if (flag == 'p')
              config.penetrationBeforeShuffle = std::stof(value);
          } catch (...) {
            std::cerr << "Invalid value for -" << flag << "\n";
            std::exit(1);
          }
          break;
        }

        default:
          std::cerr << "Unknown option: -" << flag << "\n";
          std::exit(1);
        }
      }
    }

    else {
      std::cerr << "Unknown argument: " << arg << "\n";
      std::exit(1);
    }
  }
}