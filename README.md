# Monte Carlo Blackjack Simulator (C++11)

<p align="center">

<a href="https://github.com/removingnest109/blackjackSim/actions/workflows/build.yml">
<img alt="Build" src="https://github.com/removingnest109/blackjackSim/actions/workflows/build.yml/badge.svg"/>
</a>

<a href="https://github.com/removingnest109/blackjackSim/actions/workflows/tests.yml">
<img alt="Tests" src="https://github.com/removingnest109/blackjackSim/actions/workflows/tests.yml/badge.svg"/>
</a>

</p>

![2.4 billion hands simulated in 7.7 seconds](screenshots/blackjack.png)

A high-performance Monte Carlo blackjack simulator written in portable C++11.  
Uses statistical sampling through millions of simulated hands to estimate player outcomes and strategy validity. Supports single-threaded and multithreaded simulations, hi-lo card counting, interactive mode, and detailed statistics.

## Monte Carlo Simulation

This simulator implements **Monte Carlo sampling** to estimate blackjack outcomes through statistical inference rather than analytical calculation. By simulating millions of hands with randomized card distributions, it can be used to determine:

- **Win/loss probabilities** under various strategies
- **Expected value** of betting strategies
- **Strategy convergence** - how many hands are needed for reliable estimates
- **Impact of rule variations** (e.g., dealer hitting soft 17)
- **Card counting effectiveness** through true count distribution

The law of large numbers ensures that as the number of simulated hands increases, the empirical results converge to true population parameters. With billions of hands simulated, the estimates achieve high statistical precision, making this approach superior to manual calculation for complex multi-deck scenarios.

## Features

- Simulate millions of blackjack hands.
- Configurable number of decks and hands.
- Adjustable starting bank, default bet, and shuffle penetration.
- Supports dealer hitting on soft 17.
- Optional card counting with true count betting.
- Interactive mode for step-by-step gameplay.
- Multithreading to leverage multiple CPU cores.
- Tracks detailed statistics including wins, losses, blackjacks, splits, doubles, and expected value.
- No config files, completely portable and configured by cli arguments
- Testing to ensure simulation correctness

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
| `-v`, `--verbose` | Enable verbose mode (prints detailed stats) | Disabled |
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
