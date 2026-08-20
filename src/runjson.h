#pragma once

#include "stats.h"
#include <nlohmann/json.hpp>
#include <string>

struct SimMonitor;

// Shared with the GUI so exported/imported runs use one stats encoding.
nlohmann::json statsToJson(const Stats &st);

// Write a single completed run to `path` in the {"version":1,"runs":[...]}
// shape the GUI's importRuns reads: per-thread bank timelines (rebuilt from the
// monitor's probes), aggregate stats, and derived summary figures. Reads the
// global `config` for parameters. Returns false if the file can't be opened.
bool saveRunJson(const std::string &path, const SimMonitor &monitor,
                 const Stats &result, double elapsed);
