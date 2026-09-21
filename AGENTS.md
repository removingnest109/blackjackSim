# AGENTS.md — blackjackSim

C++20 Monte Carlo blackjack simulator: one shared engine with two front ends.

## Architecture

- Engine core (`src/`, `blackjack_core` lib): `blackjack.cpp` (rules), `actions.cpp` (strategy chart), `simulation.cpp`, `config.cpp`, `model.cpp`, `runjson.cpp`, `cli.cpp`, `print.cpp`.
- CLI (`src/main.cpp` → `blackjack`): scriptable batch runs; `--save-json <file>` exports a run the GUI can import.
- GUI (`src/gui/main_gui.cpp` → `blackjack_gui`): ImGui/ImPlot desktop app with live bank-balance plotting, in-place strategy-chart editor, run-history comparison, run JSON export/import, PNG graph export.

## Build & test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target blackjack blackjack_gui
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBLACKJACK_GUI=OFF && cmake --build build --target blackjack  # CLI only, skips GLFW/ImGui/ImPlot
ctest --test-dir build   # googletest suite in tests/
```

- `BLACKJACK_GUI=ON` (default) pulls GLFW/ImGui/ImPlot/stb/portable-file-dialogs via FetchContent; `nlohmann/json` is unconditional (core run-export uses it).
- LTO uses CMP0069 NEW; keep the policy block in CMakeLists.txt.

## Conventions

- Engine, CLI, and GUI share one core — no per-front-end rule logic.
- Strategy-chart and run-JSON wire formats: see `.claude/skills/strategy-chart-runjson/SKILL.md`.
- Engine-mirror rule: rule changes here must mirror the sibling Android port at `/home/ethan/Projects/blackjack-android` (its AGENTS.md says the same in reverse) — keep game behavior in sync, never fork it.
