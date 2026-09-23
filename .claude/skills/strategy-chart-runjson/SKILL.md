---
name: strategy-chart-runjson
description: Use when loading, validating, editing, or exporting a blackjackSim custom strategy chart JSON (hard/soft/pair grids, H/S/D/P/R action codes, dealer-upcard columns, padding) or when saving, importing, or comparing run-JSON files between the CLI (--strategy, --save-json) and GUI run history.
---

## Strategy chart JSON

- Top-level object with exactly three keys: `hard`, `soft`, `pair`. Each is an array of rows; each row is an array of single-letter strings.
- Shapes (rows × cols): `hard` 22×12, `soft` 22×12, `pair` 12×12. Wrong dimensions reject the whole file; the previous chart stays active (never half-applies).
- Row index: `hard`/`soft` = player hand total; `pair` = pair rank (card value). Unused low rows (e.g. soft totals 0–12) must still be present, filled with any valid code.
- Column index = dealer upcard value with Ace = 11. Columns 0–1 are unused padding — always write 12 entries per row including them; readers index by raw card value, so dropping them shifts every decision.
- Cell codes: `H` hit, `S` stand, `D` double, `P` split, `R` surrender. Unknown codes reject the file.
- `R` semantics: surrenders only when surrender is enabled and legal (first two cards of the hand); otherwise falls back to hit. Late surrender resolves after the dealer blackjack check; early surrender before the peek (`--early-surrender` implies `--surrender`).
- CLI: `--strategy <file>` loads a chart, else built-in basic strategy. GUI: same fallback, plus a live grid editor that cycles cells through the five actions.

## Run JSON (CLI ↔ GUI interchange)

- CLI `--save-json <file>` writes one run file and suppresses the stats printout. The GUI imports it into run history with the same parameter tooltip (`describeRun`) as native GUI runs, and exports the same shape back out.
- Contents: full stats block (`hands`, `playerWins`, `dealerWins`, `playerBlackjacks`, `dealerBlackjacks`, `draw`, `shuffles`, `cardsDealt`, `splits`, `doubles`, `surrenders`, `totalBet`, `bank`) plus per-thread bank timelines (`xs`/`ys` sample hands/banks) and a cross-thread average series.
- Timeline layout: one series per table×player (`table*players + player`), rebuilt from monitor probes on save — do not assume one series per thread when players-per-table > 1.
- Round-trip rule: keep field names and the stats encoding in `runjson.h`/`runjson.cpp` as the single schema; both front ends read what the other writes.
