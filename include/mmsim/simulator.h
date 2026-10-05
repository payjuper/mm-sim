// Simulator runs one strategy against one simulated market and records what
// happened. Each 1 ms step it shows the maker the mid, takes its quotes, asks
// the fill model whether a market order reached each side during the step,
// settles those one-unit trades into cash and inventory, marks the position at
// the same mid the quotes were made against, logs the row, and only then lets
// the price move. run() is const and rebuilds the market from the seed every
// time, and it draws exactly one uniform per side per step whatever the quotes
// are, so two strategies run on the same Simulator face the identical price
// path and the identical order flow: differences between them come from the
// quoting rule, not from luck. Price and fills use separate random streams, so
// a strategy can never perturb the price path it is being tested on.
#ifndef MMSIM_SIMULATOR_H
#define MMSIM_SIMULATOR_H

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include "mmsim/fill_model.h"
#include "mmsim/market_maker.h"
#include "mmsim/price_process.h"

namespace mmsim {

// One row of the step log. Inventory, cash and pnl are after the step's fills.
struct StepRecord {
  double time = 0.0;  // seconds since the start, at the beginning of the step
  double mid = 0.0;
  double bid = 0.0;
  double ask = 0.0;
  int inventory = 0;
  double cash = 0.0;
  double pnl = 0.0;  // cash + inventory * mid
  bool bid_filled = false;
  bool ask_filled = false;
};

class Simulator {
 public:
  Simulator(const PriceProcess::Config& price, const FillModel& fills, std::uint64_t seed);

  // Simulates `steps` milliseconds from a flat position; one record per step.
  std::vector<StepRecord> run(const MarketMaker& maker, std::size_t steps) const;

 private:
  StepRecord trade(const MarketMaker& maker, const StepRecord& previous, double time,
                   double mid, std::mt19937_64& fill_rng) const;
  bool order_arrives(double distance, int size, std::mt19937_64& fill_rng) const;

  PriceProcess start_;  // the price process at time zero, copied for each run
  FillModel fills_;
  std::uint64_t seed_;
};

}  // namespace mmsim

#endif  // MMSIM_SIMULATOR_H
