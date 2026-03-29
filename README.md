# Blackjack Simulator (C++11)

A high-performance, configurable blackjack simulator written in portable C++11.  
Supports single-threaded and multithreaded simulations, hi-lo card counting, interactive mode, and detailed statistics.

---

## Features

- Simulate millions of blackjack hands.
- Configurable number of decks and hands.
- Adjustable starting bank, default bet, and shuffle penetration.
- Supports dealer hitting on soft 17.
- Optional card counting with true count betting.
- Interactive mode for step-by-step gameplay.
- Multithreading to leverage multiple CPU cores.
- Tracks detailed statistics including wins, losses, blackjacks, splits, doubles, and expected value.

---


## Build

Build using cmake:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target blackjack
```


## Configuration Options

### Short / Long Flags

| Flag | Description | Default |
|------|-------------|---------|
| `-h`, `--help` | Show help message | - |
| `-v`, `--verbose` | Enable verbose mode (prints detailed stats per thread) | Disabled |
| `-n`, `--hands <num>` | Number of hands per thread | 10,000,000 |
| `-d`, `--decks <num>` | Number of decks in shoe | 6 |
| `-b`, `--bank <amount>` | Starting bank | 100,000 |
| `-t`, `--bet <amount>` | Default bet size | 10 |
| `-p`, `--penetration <0.0-1.0>` | Shuffle penetration before reshuffle | 0.75 |
| `-s`, `--dealer-hit-soft-17` | Dealer hits on soft 17 | Disabled |
| `-i`, `--interactive` | Enable interactive mode | Disabled |
| `-c`, `--card-counting` | Enable card counting | Disabled |
| `-e`, `--debt` | Allow negative bank (debt) | Disabled |
| `-m`, `--multithread` | Enable multithreading | Disabled |

**Example:**  

```bash
./blackjack -vmc -n 1000000
# Equivalent to: verbose, multithread, card counting, 1,000,000 hands per thread
```
