#include "actions.h"

// Active strategy chart, initialised to the canonical basic-strategy default.
// Overridden before simulation by the CLI (--strategy) or the GUI editor.
StrategyTable gStrategy = kBasicStrategy;
