# Monte Carlo Blackjack Simulator (C++20)

<p align="center">

<a href="https://github.com/removingnest109/blackjackSim/actions/workflows/build.yml">
<img alt="Build" src="https://github.com/removingnest109/blackjackSim/actions/workflows/build.yml/badge.svg"/>
</a>

<a href="https://github.com/removingnest109/blackjackSim/actions/workflows/tests.yml">
<img alt="Tests" src="https://github.com/removingnest109/blackjackSim/actions/workflows/tests.yml/badge.svg"/>
</a>

</p>

![GUI version](screenshots/gui.png)

A high-performance blackjack simulator that plays out millions of hands to
estimate how a strategy actually performs — win rates, expected value, and the
long-run behavior of a bankroll. It comes in two forms that share the same
engine: an **interactive GUI** with live plotting and side-by-side run
comparison, and a **scriptable CLI** for batch experiments and automation.

## Features

- Simulate millions of hands per run across any number of independent tables in parallel, with configurable players per table.
- Configurable decks, starting bank, default bet, table minimum, and shuffle penetration.
- Bet sizing as a raw amount or a percentage of the current bank (Kelly-style proportional betting).
- Optional hi-lo card counting with true-count betting and a fully configurable bet curve.
- Dealer-hits-soft-17 rule toggle.
- Configurable strategy chart — load a custom decision chart from JSON or edit it live in the GUI.
- Late or early surrender, off by default.
- Detailed statistics: wins, losses, blackjacks, splits, doubles, surrenders, expected value, worst drawdown, and more.
- Export runs to JSON to save, share, or reopen in the GUI.

## Quick start

Grab a prebuilt release — no compiler or dependencies required.

1. Download the archive for your platform from the [**Releases**](https://github.com/removingnest109/blackjackSim/releases) page:
   - Linux: `blackjack-<version>-linux-x64.tar.gz`
   - Windows: `blackjack-<version>-windows-x64.zip`
2. Extract it. Each archive contains both programs:
   - `blackjack_gui` — the interactive desktop app
   - `blackjack` — the command-line tool

**Launch the GUI:**

```bash
./blackjack_gui        # Windows: blackjack_gui.exe
```

**Run a quick CLI simulation:**

```bash
./blackjack -vc --tables 8 -n 1000000
# verbose, card counting, 8 parallel tables, 1,000,000 hands per table
```

## The interactive GUI

A cross-platform desktop app (Windows/Linux) built with
[Dear ImGui](https://github.com/ocornut/imgui) and
[ImPlot](https://github.com/epezent/implot) for exploring betting strategies at
full simulation speed — no terminal required.

### Run simulations live

Adjust every parameter with sliders and toggles, then watch the results unfold
on a **live bank-balance graph** — one line per thread, streamed straight from
the running simulation with virtually no impact on speed (hundreds of millions
of hands per second on modern hardware). Start and stop a run whenever you like;
the stats up to the stopping point are kept. Axes are human-readable (10M, 2.4B,
1T), with an optional **normalized view** (% of starting bank) so runs with
different bankrolls can be compared fairly.

### Shape your betting strategy

- Switch between **raw bet sizing** and **percentage-of-bank** (proportional/Kelly-style) betting with a logarithmic slider.
- Set a **minimum bet** that floors the final wager in every mode.
- With card counting enabled, an **editable bet curve** lets you set the bet multiplier for each true-count bucket and design your own betting ramp.
- Edit the **strategy chart** in place — a colour-coded hard/soft/pair grid where each cell cycles through hit, stand, double, split, and surrender — and toggle **late or early surrender**. Your edits persist across sessions.

Every tracked statistic — hands, win/loss/draw rates, blackjacks, splits,
doubles, EV per hand, average bet, worst drawdown, hands per second, and more —
updates live during the run and is finalized on completion.

### Compare runs like a profiler

- Every completed run is archived to a **run history** with its full parameter set, statistics, and bank timeline.
- Overlay any combination of past runs on the graph, each with a toggleable cross-thread **average line**, to compare strategies side by side.
- **Rename runs** inline to keep experiments organized; hover any run to see the exact parameters it used.
- **Export and import runs as JSON** (all stats plus full timelines) to save or share experiments.
- **Export the current graph as a PNG**, exactly as displayed — overlays, zoom, and legend included.

## The command line

The CLI runs the same engine as a scriptable tool — ideal for batch experiments,
automation, and feeding results back into the GUI via JSON.

![CLI version](screenshots/cli.png)

```bash
./blackjack -vc --tables 8 -n 1000000
# Equivalent to: verbose, card counting, 8 parallel tables, 1,000,000 hands per table
```

Short flags can be combined (`-vc`), and `--save-json <file>` writes a run that
the GUI can import.

| Flag | Description | Default |
|------|-------------|---------|
| `-h`, `--help` | Show help message | - |
| `-v`, `--verbose` | Enable verbose mode (prints detailed stats) | Disabled |
| `-n`, `--hands <num>` | Number of hands per table | 10,000,000 |
| `-d`, `--decks <num>` | Number of decks in shoe | 6 |
| `-b`, `--bank <amount>` | Starting bank | 100,000 |
| `-t`, `--bet <amount>` | Default bet size | 10 |
| `-r`, `--bet-percent <0.0-100.0>` | Bet a percentage of current bank instead of a raw bet size | Disabled |
| `-i`, `--min-bet <amount>` | Minimum bet, floors the final bet in all modes | 1 |
| `-p`, `--penetration <0.0-1.0>` | Shuffle penetration before reshuffle | 0.75 |
| `-s`, `--dealer-hit-soft-17` | Dealer hits on soft 17 | Disabled |
| `-c`, `--card-counting` | Enable card counting | Disabled |
| `-e`, `--debt` | Allow negative bank (debt) | Disabled |
| `-o`, `--save-json <file>` | Save the run to a JSON file for GUI import (suppresses the stats printout) | Disabled |
| `--tables <num>` | Number of independent tables to simulate in parallel | 1 |
| `--players <num>` | Players per table (share one shoe and dealer) | 1 |
| `--strategy <file>` | Load a custom strategy chart from a JSON file | Basic strategy |
| `--surrender` | Allow late surrender | Disabled |
| `--early-surrender` | Allow early surrender (implies `--surrender`) | Disabled |

## Custom strategy charts

The player's decisions come from a **strategy chart** that you can replace
without touching the engine. The GUI ships an interactive editor — a
colour-coded grid where each cell cycles through the five actions — and the CLI
loads a chart from a JSON file with `--strategy <file>`. Both fall back to the
built-in basic-strategy chart when no chart is supplied.

A chart is a JSON object with three grids of single-letter action codes:

| Key | Shape | Row index | Column index |
|-----|-------|-----------|--------------|
| `hard` | 22 × 12 | hand total | dealer upcard |
| `soft` | 22 × 12 | hand total | dealer upcard |
| `pair` | 12 × 12 | pair rank | dealer upcard |

The column index is the dealer's upcard value (Ace = 11); columns 0–1 are unused
padding. Each cell is one of:

| Code | Action |
|------|--------|
| `H` | Hit |
| `S` | Stand |
| `D` | Double |
| `P` | Split |
| `R` | Surrender |

```json
{
  "hard": [ ["S", "S", ... 12 entries per row ...], ... 22 rows ... ],
  "soft": [ ... ],
  "pair": [ ... ]
}
```

Charts are validated on load: wrong grid dimensions or an unknown code are
rejected and the previous chart is left unchanged, so a malformed file never
half-applies. An `R` cell surrenders only when surrender is enabled and legal
(the first two cards of a hand); everywhere else it falls back to hitting.

### Surrender

Surrender is off by default. Enable **late surrender** with `--surrender`, which
resolves after the dealer checks for blackjack, or **early surrender** with
`--early-surrender`, which resolves before the dealer's peek and so escapes a
dealer blackjack. `--early-surrender` implies `--surrender`, and both are
toggles in the GUI. Surrendered hands are tracked as their own statistic.

## How it works

The simulator uses **Monte Carlo sampling** to estimate blackjack outcomes
through statistical inference rather than analytical calculation. By playing out
millions of hands with randomized card distributions, it estimates:

- **Win/loss probabilities** under various strategies
- **Expected value** of betting strategies
- **Strategy convergence** — how many hands are needed for reliable estimates
- **Impact of rule variations** (e.g. dealer hitting soft 17)
- **Card-counting effectiveness** through the true-count distribution

The law of large numbers guarantees that as the number of simulated hands grows,
the empirical results converge to the true underlying probabilities. With
billions of hands simulated, the estimates reach high precision — making this
approach far more practical than manual calculation for complex multi-deck
scenarios.

## Building from source

> Most users should download a [release](https://github.com/removingnest109/blackjackSim/releases)
> instead. Build from source only if you want to modify the code or run on an
> unsupported platform.

Requires CMake 3.10+ and a C++20-capable compiler (GCC 11+, Clang 13+, or
MSVC 2019 16.11+). All dependencies (GLFW, ImGui, ImPlot, nlohmann/json, stb)
are fetched automatically by CMake — no manual installs. On Linux, the GUI
needs OpenGL and X11/Wayland development headers; on Debian/Ubuntu:

```bash
sudo apt-get install libgl1-mesa-dev xorg-dev libwayland-dev libxkbcommon-dev wayland-protocols
```

On Windows it builds out of the box with MSVC.

**Build both the CLI and GUI:**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target blackjack blackjack_gui
```

The binaries are written to `build/blackjack` and `build/blackjack_gui`.

**Build only the CLI** (skips all GUI dependencies):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBLACKJACK_GUI=OFF && cmake --build build --target blackjack
```

**Build only the GUI:**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target blackjack_gui
```
