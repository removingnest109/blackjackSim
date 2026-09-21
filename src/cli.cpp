#include "cli.h"
#include "config.h"
#include <charconv>
#include <iostream>
#include <optional>
#include <string_view>

namespace {

// Strict full-string parse: unlike std::stoi/std::stof, trailing garbage
// ("10abc") is rejected rather than silently truncated.
std::optional<int> parseInt(std::string_view s) {
  int v{};
  const auto *end = s.data() + s.size();
  const auto [ptr, ec] = std::from_chars(s.data(), end, v);
  if (ec == std::errc{} && ptr == end)
    return v;
  return std::nullopt;
}

std::optional<float> parseFloat(std::string_view s) {
  float v{};
  const auto *end = s.data() + s.size();
  const auto [ptr, ec] = std::from_chars(s.data(), end, v);
  if (ec == std::errc{} && ptr == end)
    return v;
  return std::nullopt;
}

} // namespace

void printHelp() {
  std::cout
      << "Options:\n"
         "  -h, --help                     Show help\n"
         "  -v, --verbose                  Enable verbose mode\n"
         "  -n, --hands <num>              Number of hands (default 10000000)\n"
         "  -d, --decks <num>              Number of decks (default 6)\n"
         "  -b, --bank <amount>            Starting bank (default 100000)\n"
         "  -t, --bet <amount>             Default bet size (default 10)\n"
         "  -r, --bet-percent <0.0-100.0>  Bet a percentage of current bank\n"
         "                                 instead of a raw bet size\n"
         "  -i, --min-bet <amount>         Minimum bet, floors the final bet\n"
         "                                 in all modes (default 1)\n"
         "      --max-bet <amount>         Maximum bet, caps the final bet\n"
         "                                 (0 = no limit, default 0)\n"
         "  -p, --penetration <0.0-1.0>    Shuffle penetration (default 0.75)\n"
         "  -s, --dealer-hit-soft-17       Dealer hits soft 17\n"
         "      --blackjack-payout <ratio> Blackjack pay table: 3:2, 6:5\n"
         "                                 or 1:1 (default 3:2)\n"
         "  -c, --card-counting            Enable card counting\n"
         "  -e, --debt                     Enable negative bank\n"
         "  -o, --save-json <file>         Save the run to <file> as JSON for\n"
         "                                 GUI import (suppresses stats output)\n"
         "      --tables <num>             Number of tables to simulate in\n"
         "                                 parallel (default 1)\n"
         "      --players <num>            Players per table (default 1)\n"
         "      --strategy <file>          Load a custom strategy chart (JSON)\n"
         "      --surrender                Allow late surrender\n"
         "      --early-surrender          Allow early surrender (implies\n"
         "                                 --surrender)\n";
}

void getArgs(const int argc, char **argv) {
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];

    if (arg.rfind("--", 0) == 0) {
      if (arg == "--help") {
        printHelp();
        std::exit(0);
      }
      if (arg == "--verbose")
        config.verbose = true;
      else if (arg == "--dealer-hit-soft-17")
        config.dealerHitSoft17 = true;
      else if (arg == "--card-counting")
        config.cardCounting = true;
      else if (arg == "--debt")
        config.debtAllowed = true;
      else if (arg == "--surrender")
        config.surrenderAllowed = true;
      else if (arg == "--early-surrender") {
        config.surrenderAllowed = true;
        config.earlySurrender = true;
      }

      else if (arg == "--save-json") {
        if (i + 1 >= argc) {
          std::cerr << "Missing value for " << arg << "\n";
          std::exit(1);
        }
        config.saveJsonPath = argv[++i];
      }

      else if (arg == "--blackjack-payout") {
        if (i + 1 >= argc) {
          std::cerr << "Missing value for " << arg << "\n";
          std::exit(1);
        }
        const std::string_view value = argv[++i];
        if (value == "3:2")
          config.blackjackPayout = BlackjackPayout::ThreeToTwo;
        else if (value == "6:5")
          config.blackjackPayout = BlackjackPayout::SixToFive;
        else if (value == "1:1")
          config.blackjackPayout = BlackjackPayout::EvenMoney;
        else {
          std::cerr << "Invalid value for " << arg << " (want 3:2, 6:5, 1:1)\n";
          std::exit(1);
        }
      }

      else if (arg == "--strategy") {
        if (i + 1 >= argc) {
          std::cerr << "Missing value for " << arg << "\n";
          std::exit(1);
        }
        config.strategyPath = argv[++i];
      }

      else if (arg == "--tables" || arg == "--players") {
        if (i + 1 >= argc) {
          std::cerr << "Missing value for " << arg << "\n";
          std::exit(1);
        }
        const auto v = parseInt(argv[++i]);
        if (!v || *v < 1) {
          std::cerr << "Invalid value for " << arg << "\n";
          std::exit(1);
        }
        if (arg == "--tables")
          config.threads = static_cast<unsigned int>(*v);
        else
          config.playersPerTable = *v;
      }

      else if (arg == "--hands" || arg == "--decks" || arg == "--bank" ||
               arg == "--bet" || arg == "--penetration" ||
               arg == "--bet-percent" || arg == "--min-bet" ||
               arg == "--max-bet") {
        if (i + 1 >= argc) {
          std::cerr << "Missing value for " << arg << "\n";
          std::exit(1);
        }
        const std::string_view value = argv[++i];
        bool ok = true;
        const auto asInt = [&](int &dst) {
          if (const auto v = parseInt(value))
            dst = *v;
          else
            ok = false;
        };
        if (arg == "--hands")
          asInt(config.numberHands);
        else if (arg == "--decks")
          asInt(config.numberDecks);
        else if (arg == "--bank")
          asInt(config.startingBank);
        else if (arg == "--bet")
          asInt(config.defaultBetSize);
        else if (arg == "--penetration") {
          if (const auto v = parseFloat(value))
            config.penetrationBeforeShuffle = *v;
          else
            ok = false;
        } else if (arg == "--bet-percent") {
          if (const auto v = parseFloat(value)) {
            config.betPercent = *v;
            config.betPercentMode = true;
          } else
            ok = false;
        } else if (arg == "--min-bet")
          asInt(config.minimumBet);
        else if (arg == "--max-bet")
          asInt(config.maximumBet);
        if (!ok) {
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
        case 'c':
          config.cardCounting = true;
          break;
        case 'e':
          config.debtAllowed = true;
          break;

        case 'o': {
          if (j + 1 != arg.size()) {
            std::cerr << "Option -" << flag << " requires a separate value\n";
            std::exit(1);
          }
          if (i + 1 >= argc) {
            std::cerr << "Missing value for -" << flag << "\n";
            std::exit(1);
          }
          config.saveJsonPath = argv[++i];
          break;
        }

        case 'n':
        case 'd':
        case 'b':
        case 't':
        case 'p':
        case 'r':
        case 'i': {
          if (j + 1 != arg.size()) {
            std::cerr << "Option -" << flag << " requires a separate value\n";
            std::exit(1);
          }
          if (i + 1 >= argc) {
            std::cerr << "Missing value for -" << flag << "\n";
            std::exit(1);
          }
          const std::string_view value = argv[++i];
          bool ok = true;
          const auto asInt = [&](int &dst) {
            if (const auto v = parseInt(value))
              dst = *v;
            else
              ok = false;
          };
          if (flag == 'n')
            asInt(config.numberHands);
          else if (flag == 'd')
            asInt(config.numberDecks);
          else if (flag == 'b')
            asInt(config.startingBank);
          else if (flag == 't')
            asInt(config.defaultBetSize);
          else if (flag == 'p') {
            if (const auto v = parseFloat(value))
              config.penetrationBeforeShuffle = *v;
            else
              ok = false;
          } else if (flag == 'r') {
            if (const auto v = parseFloat(value)) {
              config.betPercent = *v;
              config.betPercentMode = true;
            } else
              ok = false;
          } else if (flag == 'i')
            asInt(config.minimumBet);
          if (!ok) {
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