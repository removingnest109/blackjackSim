#pragma once

#include <string>
#include "model.h"
#include "stats.h"

void printGlobalVars();
void printStats(const Stats &stats);
void announceIfInteractive(const std::string &message);
void printHandState(const std::string &label, const Hand &hand);