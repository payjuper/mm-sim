#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "mmsim/fill_model.h"
#include "mmsim/inventory_skew_mm.h"
#include "mmsim/price_process.h"
#include "mmsim/simulator.h"
#include "mmsim/stats.h"

using Catch::Matchers::WithinAbs;
using mmsim::AggregateStats;
using mmsim::RunStats;
using mmsim::StepRecord;

namespace {

StepRecord make_record(double pnl, int inventory, bool bid_filled, bool ask_filled) {
  StepRecord record;
  record.pnl = pnl;
  record.inventory = inventory;
  record.bid_filled = bid_filled;
  record.ask_filled = ask_filled;
  return record;
}

}  // namespace

TEST_CASE("Stats: summarize matches a hand-computed run", "[stats]") {
  // P&L path 1, 3, 2, 0, 4 from a start of 0: step P&Ls are 1, 2, -1, -2, 4.
  const std::vector<StepRecord> log = {
      make_record(1.0, 1, true, false),  make_record(3.0, 2, true, false),
      make_record(2.0, -3, false, true), make_record(0.0, 1, true, true),
      make_record(4.0, 0, false, false)};
  const RunStats stats = mmsim::summarize(log);

  CHECK(stats.final_pnl == 4.0);
  CHECK_THAT(stats.mean_step_pnl, WithinAbs(0.8, 1e-12));
  // Squared deviations from 0.8 sum to 22.8; divided by n - 1 = 4 that is 5.7.
  CHECK_THAT(stats.std_step_pnl, WithinAbs(std::sqrt(5.7), 1e-12));
  CHECK_THAT(stats.sharpe, WithinAbs(0.8 / std::sqrt(5.7), 1e-12));
  CHECK(stats.max_drawdown == 3.0);  // from the peak of 3 down to 0
  CHECK(stats.max_abs_inventory == 3.0);
  CHECK(stats.fill_count == 5.0);
}

TEST_CASE("Stats: drawdown is measured from the starting P&L of zero", "[stats]") {
  const std::vector<StepRecord> log = {make_record(-1.0, 0, false, false),
                                       make_record(-3.0, 0, false, false),
                                       make_record(-2.0, 0, false, false)};
  CHECK(mmsim::summarize(log).max_drawdown == 3.0);
}

TEST_CASE("Stats: an empty or flat run gives zeros, not NaN", "[stats]") {
  const RunStats empty = mmsim::summarize({});
  CHECK(empty.final_pnl == 0.0);
  CHECK(empty.sharpe == 0.0);

  const std::vector<StepRecord> flat(10, make_record(0.0, 0, false, false));
  const RunStats stats = mmsim::summarize(flat);
  CHECK(stats.std_step_pnl == 0.0);
  CHECK(stats.sharpe == 0.0);
  CHECK(stats.max_drawdown == 0.0);
}

TEST_CASE("Stats: aggregate averages runs and reports the spread of final P&L", "[stats]") {
  RunStats first;
  first.final_pnl = 10.0;
  first.max_abs_inventory = 4.0;
  first.fill_count = 100.0;
  RunStats second;
  second.final_pnl = 14.0;
  second.max_abs_inventory = 8.0;
  second.fill_count = 300.0;

  const AggregateStats result = mmsim::aggregate({first, second});
  CHECK(result.runs == 2u);
  CHECK_THAT(result.mean.final_pnl, WithinAbs(12.0, 1e-12));
  CHECK_THAT(result.mean.max_abs_inventory, WithinAbs(6.0, 1e-12));
  CHECK_THAT(result.mean.fill_count, WithinAbs(200.0, 1e-12));
  CHECK_THAT(result.std_final_pnl, WithinAbs(std::sqrt(8.0), 1e-12));
}

TEST_CASE("Stats: aggregate handles one run and no runs", "[stats]") {
  RunStats only;
  only.final_pnl = 7.0;
  const AggregateStats single = mmsim::aggregate({only});
  CHECK(single.mean.final_pnl == 7.0);
  CHECK(single.std_final_pnl == 0.0);

  const AggregateStats none = mmsim::aggregate({});
  CHECK(none.runs == 0u);
  CHECK(none.mean.final_pnl == 0.0);
}

TEST_CASE("Stats: summarize agrees with a simulated log", "[stats]") {
  mmsim::PriceProcess::Config price;
  price.volatility = 3.0;
  const mmsim::Simulator simulator(price, mmsim::FillModel(50.0, 20.0), 4);
  const std::vector<StepRecord> log = simulator.run(mmsim::InventorySkewMM(0.05, 0.01, 1), 5000);

  int fills = 0;
  int max_abs_inventory = 0;
  for (const StepRecord& record : log) {
    fills += static_cast<int>(record.bid_filled) + static_cast<int>(record.ask_filled);
    max_abs_inventory = std::max(max_abs_inventory, std::abs(record.inventory));
  }
  const RunStats stats = mmsim::summarize(log);
  REQUIRE(fills > 50);
  CHECK(stats.final_pnl == log.back().pnl);
  CHECK(stats.fill_count == fills);
  CHECK(stats.max_abs_inventory == max_abs_inventory);
  CHECK(stats.max_drawdown >= 0.0);
}
