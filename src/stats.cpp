#include "mmsim/stats.h"

#include <algorithm>
#include <cmath>

namespace mmsim {

namespace {

// Sample standard deviation (n - 1); zero when there are fewer than two values.
double sample_std(double squared_deviations, std::size_t count) {
  return count > 1 ? std::sqrt(squared_deviations / static_cast<double>(count - 1)) : 0.0;
}

}  // namespace

RunStats summarize(const std::vector<StepRecord>& log) {
  RunStats stats;
  if (log.empty()) {
    return stats;
  }
  stats.final_pnl = log.back().pnl;
  // Step P&Ls telescope to the final P&L, so their mean needs no separate pass.
  stats.mean_step_pnl = stats.final_pnl / static_cast<double>(log.size());

  double previous_pnl = 0.0;
  double peak_pnl = 0.0;
  double squared_deviations = 0.0;
  for (const StepRecord& record : log) {
    const double deviation = record.pnl - previous_pnl - stats.mean_step_pnl;
    squared_deviations += deviation * deviation;
    previous_pnl = record.pnl;
    peak_pnl = std::max(peak_pnl, record.pnl);
    stats.max_drawdown = std::max(stats.max_drawdown, peak_pnl - record.pnl);
    stats.max_abs_inventory =
        std::max(stats.max_abs_inventory, std::abs(static_cast<double>(record.inventory)));
    stats.fill_count += static_cast<int>(record.bid_filled) + static_cast<int>(record.ask_filled);
  }
  stats.std_step_pnl = sample_std(squared_deviations, log.size());
  stats.sharpe = stats.std_step_pnl > 0.0 ? stats.mean_step_pnl / stats.std_step_pnl : 0.0;
  return stats;
}

AggregateStats aggregate(const std::vector<RunStats>& runs) {
  AggregateStats result;
  result.runs = runs.size();
  double RunStats::* const fields[] = {
      &RunStats::final_pnl,    &RunStats::mean_step_pnl,     &RunStats::std_step_pnl,
      &RunStats::sharpe,       &RunStats::max_drawdown,      &RunStats::max_abs_inventory,
      &RunStats::fill_count};
  const double count = static_cast<double>(runs.size());
  for (const RunStats& run : runs) {
    for (const auto field : fields) {
      result.mean.*field += run.*field / count;
    }
  }
  double squared_deviations = 0.0;
  for (const RunStats& run : runs) {
    const double deviation = run.final_pnl - result.mean.final_pnl;
    squared_deviations += deviation * deviation;
  }
  result.std_final_pnl = sample_std(squared_deviations, runs.size());
  return result;
}

}  // namespace mmsim
