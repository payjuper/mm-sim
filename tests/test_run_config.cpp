#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "mmsim/flags.h"
#include "mmsim/market_maker.h"
#include "mmsim/price_process.h"
#include "mmsim/run_config.h"
#include "mmsim/simulator.h"

using Catch::Matchers::WithinAbs;
using mmsim::Quotes;
using mmsim::RunConfig;
using mmsim::StepRecord;

namespace {

constexpr double kMid = 100.0;

// A short, busy run: five seconds with enough order flow to fill often.
RunConfig busy_config(const char* strategy) {
  RunConfig config;
  config.strategy = strategy;
  config.horizon = 5.0;
  config.base_intensity = 100.0;
  return config;
}

}  // namespace

TEST_CASE("RunConfig: flags override the defaults they name and no others", "[run_config]") {
  const std::vector<const char*> arguments = {"program", "--strategy", "as", "--vol", "0.5",
                                              "--k",     "30",         "--A",  "2",     "--seed",
                                              "9"};
  const mmsim::Flags flags(static_cast<int>(arguments.size()), arguments.data(),
                           mmsim::run_flag_names());
  const RunConfig config = mmsim::run_config_from_flags(flags);
  const RunConfig defaults;

  CHECK(config.strategy == "as");
  CHECK(config.volatility == 0.5);
  CHECK(config.decay == 30.0);
  CHECK(config.base_intensity == 2.0);
  CHECK(config.seed == 9u);
  CHECK(config.horizon == defaults.horizon);
  CHECK(config.spread == defaults.spread);
  CHECK(config.skew == defaults.skew);
  CHECK(config.gamma == defaults.gamma);
}

TEST_CASE("RunConfig: --spread is the full width of the fixed-spread strategies",
          "[run_config]") {
  RunConfig config;
  config.spread = 0.2;
  config.skew = 0.02;

  config.strategy = "symmetric";
  const Quotes symmetric = mmsim::make_maker(config)->quote(kMid, 3, 0.0);
  CHECK_THAT(symmetric.bid.price, WithinAbs(99.9, 1e-12));
  CHECK_THAT(symmetric.ask.price, WithinAbs(100.1, 1e-12));

  // Long 3 units at 0.02 per unit moves both quotes down by 0.06.
  config.strategy = "skew";
  const Quotes skewed = mmsim::make_maker(config)->quote(kMid, 3, 0.0);
  CHECK_THAT(skewed.bid.price, WithinAbs(99.84, 1e-12));
  CHECK_THAT(skewed.ask.price, WithinAbs(100.04, 1e-12));
}

TEST_CASE("RunConfig: the as strategy gets volatility in price units per sqrt(second)",
          "[run_config]") {
  RunConfig config;
  config.strategy = "as";
  config.volatility = 0.4;
  config.gamma = 0.5;
  config.decay = 20.0;
  config.horizon = 300.0;
  const Quotes quotes = mmsim::make_maker(config)->quote(kMid, 2, 0.0);

  const mmsim::PriceProcess::Config price;
  const double sigma = price.initial_price * 0.4 / std::sqrt(price.seconds_per_year);
  const double risk_term = 0.5 * sigma * sigma * 300.0;  // gamma * sigma^2 * (T - t)
  const double centre = 0.5 * (quotes.bid.price + quotes.ask.price);
  CHECK_THAT(centre, WithinAbs(kMid - 2 * risk_term, 1e-12));
  CHECK_THAT(quotes.ask.price - quotes.bid.price,
             WithinAbs(risk_term + 2.0 / 0.5 * std::log1p(0.5 / 20.0), 1e-12));
}

TEST_CASE("RunConfig: an unknown strategy is rejected", "[run_config]") {
  RunConfig config;
  config.strategy = "martingale";
  CHECK_THROWS_AS(mmsim::make_maker(config), std::invalid_argument);
  CHECK_THROWS_AS(mmsim::simulate(config), std::invalid_argument);
}

TEST_CASE("RunConfig: set_parameter sets by flag name and refuses a parameter with no effect",
          "[run_config]") {
  RunConfig config;
  config.strategy = "as";
  mmsim::set_parameter(config, "gamma", 0.9);
  mmsim::set_parameter(config, "k", 15.0);
  CHECK(config.gamma == 0.9);
  CHECK(config.decay == 15.0);

  CHECK_THROWS_AS(mmsim::set_parameter(config, "spread", 0.2), std::invalid_argument);
  CHECK_THROWS_AS(mmsim::set_parameter(config, "skew", 0.02), std::invalid_argument);
  CHECK_THROWS_AS(mmsim::set_parameter(config, "seed", 3.0), std::invalid_argument);
  config.strategy = "skew";
  CHECK_THROWS_AS(mmsim::set_parameter(config, "gamma", 0.9), std::invalid_argument);
}

TEST_CASE("RunConfig: simulate logs one step per millisecond of horizon", "[run_config]") {
  RunConfig config = busy_config("symmetric");
  config.horizon = 2.5;
  CHECK(mmsim::simulate(config).size() == 2500u);

  for (const double bad : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()}) {
    config.horizon = bad;
    CHECK_THROWS_AS(mmsim::simulate(config), std::invalid_argument);
  }
}

TEST_CASE("RunConfig: the seed decides the run", "[run_config]") {
  RunConfig config = busy_config("as");
  const std::vector<StepRecord> first = mmsim::simulate(config);
  const std::vector<StepRecord> again = mmsim::simulate(config);
  config.seed += 1;
  const std::vector<StepRecord> other = mmsim::simulate(config);

  CHECK(first.back().pnl == again.back().pnl);
  CHECK(first.back().inventory == again.back().inventory);
  CHECK(first.back().pnl != other.back().pnl);
}

TEST_CASE("RunConfig: skew with zero skew is the symmetric strategy, step for step",
          "[run_config]") {
  RunConfig skewed = busy_config("skew");
  skewed.skew = 0.0;
  const std::vector<StepRecord> symmetric_log = mmsim::simulate(busy_config("symmetric"));
  const std::vector<StepRecord> skewed_log = mmsim::simulate(skewed);

  REQUIRE(symmetric_log.size() == skewed_log.size());
  REQUIRE(symmetric_log.back().pnl != 0.0);  // the run did trade
  for (std::size_t i = 0; i < symmetric_log.size(); ++i) {
    REQUIRE(symmetric_log[i].inventory == skewed_log[i].inventory);
    REQUIRE(symmetric_log[i].pnl == skewed_log[i].pnl);
  }
}
