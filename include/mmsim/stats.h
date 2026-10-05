// Stats reduces a run's step log to the few numbers used to compare strategies:
// what the run earned, how bumpy the ride was, and how much inventory it took
// to get there. Everything is computed from the log alone, so any reported
// figure can be recomputed from path.csv and the simulator needs no statistics
// code of its own. RunStats holds doubles throughout, counts included, so that
// the average of many runs is itself a RunStats; aggregate() returns that
// average plus the one figure that only exists across runs, the standard
// deviation of final P&L from seed to seed. The Sharpe-like ratio is per step
// (mean over standard deviation of step P&L) and is deliberately not annualised.
#ifndef MMSIM_STATS_H
#define MMSIM_STATS_H

#include <cstddef>
#include <vector>

#include "mmsim/simulator.h"

namespace mmsim {

struct RunStats {
  double final_pnl = 0.0;
  double mean_step_pnl = 0.0;
  double std_step_pnl = 0.0;
  double sharpe = 0.0;        // mean_step_pnl / std_step_pnl, 0 when the std is 0
  double max_drawdown = 0.0;  // largest fall of P&L from its running peak
  double max_abs_inventory = 0.0;
  double fill_count = 0.0;
};

struct AggregateStats {
  std::size_t runs = 0;
  RunStats mean;               // each field averaged across runs
  double std_final_pnl = 0.0;  // across runs
};

// Statistics of one run. P&L starts from zero before the first step.
RunStats summarize(const std::vector<StepRecord>& log);

// Statistics across runs, typically the same configuration under different seeds.
AggregateStats aggregate(const std::vector<RunStats>& runs);

}  // namespace mmsim

#endif  // MMSIM_STATS_H
