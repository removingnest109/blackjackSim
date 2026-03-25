#pragma once

#include <string>

struct Stats;
struct Hand;
struct Config;

void printGlobalVars(const Config &c);
void printStats(const Stats &stats, const Config &c);
void announceIfInteractive(const std::string &message, const Config &c);
void printHandState(const std::string &label, const Hand &hand);