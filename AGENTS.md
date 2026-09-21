# AGENTS.md — blackjackSim

High-performance Monte Carlo blackjack simulator (C++20). Shares its engine with the `blackjack-android` app — changes here are the source of truth for rules.

## Architecture

- **Engine** (`src/`): rule model, action logic, betting (raw / percentage-of-bank), hi-lo card counting with a configurable bet curve, and the multithreaded simulator core.
- **Two front-ends, one engine**: `blackjack` (CLI) and `blackjack_gui` (Dear ImGui + ImPlot desktop app). Both consume the same simulation/stats code.
- **Run records**: every run is archived with full parameters + bank timeline; export/import as JSON (`--save-json`), PNG graph export in the GUI.

## Build & test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target blackjack blackjack_gui
# GUI-only (skip, costs GLFW/GL deps): -DBLACKJACK_GUI=OFF builds just `blackjack`
```

- All deps (GLFW, ImGui, ImPlot, nlohmann/json, stb) are fetched by CMake — no manual installs. On Linux the GUI needs `libgl1-mesa-dev xorg-dev libwayland-dev libxkbcommon-dev wayland-protocols`.
- Tests: `ctest --test-dir build --output-on-failure` (tests live under `tests/`).
- CLI flags are composite (`-vmc` = verbose + multithread + card-counting); `-n` is hands **per thread**. Defaults: 6 decks, bank 100k, bet 10, penetration 0.75, dealer stands soft 17.

## Skills

- `cpp-conventions` — CMake presets, toolchain, ctest.
- `performance-method` — measure-first protocol; the multithreaded core is the hot path.
- `testing-discipline` — writing and reading the ctest suite.

## Conventions

- Rule/statistics changes must stay identical across the CLI, GUI, and the JNI core in `../blackjack-android` — that app reads this engine, it does not fork it.
- Keep the engine single-threaded-friendly and race-free; multithreading is per-thread simulations merged at the end.
- Don't bloat the fetch-listed deps; keep the CMake boilerplate lighter than the engine.