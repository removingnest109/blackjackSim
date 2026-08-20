#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

// Cross-series average of index-aligned bank samples (sample i is published on
// the same iteration by every table). Each output x is the max across series:
// a bankrupted table stops playing, so its own hands counter — and sampled x —
// freezes, and taking any single series' x would stall the average line there.
// The max only freezes once every table has stopped, which is when the average
// genuinely stops advancing.
inline void buildAverageSeries(const std::vector<std::vector<double>> &xs,
                               const std::vector<std::vector<double>> &ys,
                               std::vector<double> &avgX,
                               std::vector<double> &avgY) {
  avgX.clear();
  avgY.clear();
  if (xs.empty() || ys.empty())
    return;
  size_t n = xs[0].size();
  for (size_t t = 1; t < xs.size(); ++t)
    n = std::min(n, xs[t].size());
  for (size_t t = 0; t < ys.size(); ++t)
    n = std::min(n, ys[t].size());
  avgX.reserve(n);
  avgY.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    double x = xs[0][i];
    for (size_t t = 1; t < xs.size(); ++t)
      x = std::max(x, xs[t][i]);
    double sumY = 0.0;
    for (size_t t = 0; t < ys.size(); ++t)
      sumY += ys[t][i];
    avgX.push_back(x);
    avgY.push_back(sumY / static_cast<double>(ys.size()));
  }
}

// Largest peak-to-trough drop in a bank series, as a positive magnitude.
inline double maxDrawdown(const std::vector<double> &series) {
  double peak = series.empty() ? 0.0 : series[0];
  double worst = 0.0;
  for (double v : series) {
    if (v > peak)
      peak = v;
    worst = std::max(worst, peak - v);
  }
  return worst;
}
