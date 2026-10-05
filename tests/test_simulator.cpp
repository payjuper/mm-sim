#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "mmsim/avellaneda_stoikov_mm.h"
#include "mmsim/fill_model.h"
#include "mmsim/inventory_skew_mm.h"
#include "mmsim/market_maker.h"
#include "mmsim/price_process.h"
#include "mmsim/simulator.h"
#include "mmsim/symmetric_mm.h"

using Catch::Matchers::WithinAbs;
using mmsim::FillModel;
using mmsim::PriceProcess;
using mmsim::Simulator;
using mmsim::StepRecord;
using mmsim::SymmetricMM;

namespace {

constexpr double kDecay = 20.0;
constexpr std::size_t kSteps = 5000;  // five simulated seconds

// Quotes fixed offsets from the mid with chosen sizes. A negative offset puts
// the quote through the mid; a size of 0 pulls that side.
class OffsetMaker final : public mmsim::MarketMaker {
 public:
  OffsetMaker(double bid_offset, int bid_size, double ask_offset, int ask_size)
      : bid_offset_(bid_offset), bid_size_(bid_size), ask_offset_(ask_offset),
        ask_size_(ask_size) {}

  mmsim::Quotes quote(double mid, int /*inventory*/, double /*time*/) const override {
    return {{mid - bid_offset_, bid_size_}, {mid + ask_offset_, ask_size_}};
  }

 private:
  double bid_offset_;
  int bid_size_;
  double ask_offset_;
  int ask_size_;
};

// A lively market: volatile enough to move many ticks, busy enough to fill often.
Simulator make_simulator(std::uint64_t seed, double base_intensity = 50.0) {
  PriceProcess::Config price;
  price.volatility = 3.0;
  return Simulator(price, FillModel(base_intensity, kDecay), seed);
}

std::size_t count_fills(const std::vector<StepRecord>& log) {
  std::size_t fills = 0;
  for (const StepRecord& record : log) {
    fills += static_cast<std::size_t>(record.bid_filled) +
             static_cast<std::size_t>(record.ask_filled);
  }
  return fills;
}

std::vector<double> column(const std::vector<StepRecord>& log, double StepRecord::* field) {
  std::vector<double> values;
  for (const StepRecord& record : log) {
    values.push_back(record.*field);
  }
  return values;
}

}  // namespace

TEST_CASE("Simulator: cash + inventory * mid equals reported P&L at every step", "[simulator]") {
  mmsim::AvellanedaStoikovMM::Params params;
  params.gamma = 0.5;
  params.sigma = 0.12;
  params.k = kDecay;
  params.horizon = static_cast<double>(kSteps) * PriceProcess::kStepSeconds;

  std::vector<std::unique_ptr<mmsim::MarketMaker>> makers;
  makers.push_back(std::make_unique<SymmetricMM>(0.05, 1));
  makers.push_back(std::make_unique<mmsim::InventorySkewMM>(0.05, 0.02, 1));
  makers.push_back(std::make_unique<mmsim::AvellanedaStoikovMM>(params));

  for (const auto& maker : makers) {
    const std::vector<StepRecord> log = make_simulator(3).run(*maker, kSteps);
    REQUIRE(count_fills(log) > 50u);

    // Rebuild the books from the fill flags alone, independently of the simulator.
    int inventory = 0;
    double cash = 0.0;
    for (const StepRecord& record : log) {
      if (record.bid_filled) {
        inventory += 1;
        cash -= record.bid;
      }
      if (record.ask_filled) {
        inventory -= 1;
        cash += record.ask;
      }
      REQUIRE(record.inventory == inventory);
      REQUIRE_THAT(record.cash, WithinAbs(cash, 1e-9));
      REQUIRE_THAT(record.pnl, WithinAbs(record.cash + record.inventory * record.mid, 1e-9));
    }
  }
}

TEST_CASE("Simulator: a maker with infinite spread never fills", "[simulator]") {
  const SymmetricMM maker(std::numeric_limits<double>::infinity(), 1);
  const std::vector<StepRecord> log = make_simulator(5).run(maker, kSteps);

  CHECK(count_fills(log) == 0u);
  for (const StepRecord& record : log) {
    REQUIRE(record.inventory == 0);
    REQUIRE(record.cash == 0.0);
    REQUIRE(record.pnl == 0.0);
  }
}

TEST_CASE("Simulator: logs one record per millisecond", "[simulator]") {
  const std::vector<StepRecord> log = make_simulator(1).run(SymmetricMM(0.05, 1), kSteps);

  REQUIRE(log.size() == kSteps);
  CHECK(log.front().time == 0.0);
  CHECK(log.front().mid == 100.0);
  CHECK_THAT(log.back().time, WithinAbs(4.999, 1e-9));
}

TEST_CASE("Simulator: same seed reproduces the run, another seed does not", "[simulator]") {
  const SymmetricMM maker(0.05, 1);
  const std::vector<StepRecord> first = make_simulator(8).run(maker, kSteps);
  const std::vector<StepRecord> again = make_simulator(8).run(maker, kSteps);
  const std::vector<StepRecord> other = make_simulator(9).run(maker, kSteps);

  CHECK(column(first, &StepRecord::mid) == column(again, &StepRecord::mid));
  CHECK(column(first, &StepRecord::pnl) == column(again, &StepRecord::pnl));
  CHECK(column(first, &StepRecord::mid) != column(other, &StepRecord::mid));
  CHECK(column(first, &StepRecord::pnl) != column(other, &StepRecord::pnl));
}

TEST_CASE("Simulator: strategies on one simulator share the price path and order flow",
          "[simulator]") {
  const Simulator simulator = make_simulator(21);
  const std::vector<StepRecord> wide = simulator.run(SymmetricMM(0.05, 1), kSteps);
  const std::vector<StepRecord> tight = simulator.run(SymmetricMM(0.02, 1), kSteps);

  CHECK(column(wide, &StepRecord::mid) == column(tight, &StepRecord::mid));
  // Same uniforms, higher fill probability: every fill of the wide quote is
  // also a fill of the tight one.
  for (std::size_t i = 0; i < kSteps; ++i) {
    REQUIRE((!wide[i].bid_filled || tight[i].bid_filled));
    REQUIRE((!wide[i].ask_filled || tight[i].ask_filled));
  }
  CHECK(count_fills(tight) > count_fills(wide));
}

TEST_CASE("Simulator: a quote far through the mid fills exactly once per step", "[simulator]") {
  // The ask sits 1.00 below the mid, so lambda * dt is in the millions. The fill
  // probability must cap at 1: one unit sold per step, never more, and the
  // pulled bid (size 0) must never trade.
  const OffsetMaker maker(0.05, 0, -1.0, 1);
  const std::vector<StepRecord> log = make_simulator(13).run(maker, kSteps);

  double cash = 0.0;
  for (std::size_t i = 0; i < kSteps; ++i) {
    cash += log[i].mid - 1.0;
    REQUIRE(log[i].ask_filled);
    REQUIRE_FALSE(log[i].bid_filled);
    REQUIRE(log[i].inventory == -static_cast<int>(i + 1));
    REQUIRE_THAT(log[i].cash, WithinAbs(cash, 1e-6));
  }
}

TEST_CASE("Simulator: step fill frequency is 1 - exp(-lambda * dt), not lambda * dt",
          "[simulator]") {
  // A = 1000 per second quoted at the mid gives lambda * dt = 1. The linear
  // approximation would fill on every step; the exact probability is 1 - 1/e.
  const std::size_t steps = 100000;
  const OffsetMaker maker(0.0, 1, 0.0, 1);
  const std::vector<StepRecord> log = make_simulator(17, 1000.0).run(maker, steps);

  std::size_t bid_fills = 0;
  std::size_t ask_fills = 0;
  for (const StepRecord& record : log) {
    bid_fills += static_cast<std::size_t>(record.bid_filled);
    ask_fills += static_cast<std::size_t>(record.ask_filled);
  }
  const double expected = 1.0 - std::exp(-1.0);  // about 0.632
  // The sampling error of each frequency is about 0.0015 at this sample size.
  CHECK_THAT(static_cast<double>(bid_fills) / static_cast<double>(steps),
             WithinAbs(expected, 0.01));
  CHECK_THAT(static_cast<double>(ask_fills) / static_cast<double>(steps),
             WithinAbs(expected, 0.01));
}
