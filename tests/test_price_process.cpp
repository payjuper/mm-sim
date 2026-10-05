#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "mmsim/price_process.h"

using mmsim::PriceProcess;

namespace {

std::vector<double> simulate(const PriceProcess::Config& config, std::uint64_t seed,
                             std::size_t steps) {
  PriceProcess process(config, seed);
  std::vector<double> path;
  path.reserve(steps);
  for (std::size_t i = 0; i < steps; ++i) {
    path.push_back(process.step());
  }
  return path;
}

}  // namespace

TEST_CASE("PriceProcess: same seed gives identical paths", "[price_process]") {
  PriceProcess::Config config;
  config.tick_size = 0.0;  // compare unrounded prices so the check is bit-exact

  CHECK(simulate(config, 42, 10000) == simulate(config, 42, 10000));
  CHECK(simulate(config, 42, 10000) != simulate(config, 43, 10000));
}

TEST_CASE("PriceProcess: zero volatility gives a flat path", "[price_process]") {
  PriceProcess::Config config;
  config.volatility = 0.0;

  const std::vector<double> path = simulate(config, 7, 10000);
  CHECK(std::all_of(path.begin(), path.end(),
                    [&](double price) { return price == config.initial_price; }));
}

TEST_CASE("PriceProcess: mid starts at the initial price", "[price_process]") {
  PriceProcess::Config config;
  CHECK(PriceProcess(config, 1).mid() == config.initial_price);
}

TEST_CASE("PriceProcess: mid stays on the tick grid", "[price_process]") {
  PriceProcess::Config config;
  config.volatility = 3.0;  // large enough to cross many ticks in 20 seconds
  config.tick_size = 0.05;

  const std::vector<double> path = simulate(config, 11, 20000);
  CHECK(std::all_of(path.begin(), path.end(), [&](double price) {
    const double ticks = price / config.tick_size;
    return std::abs(ticks - std::round(ticks)) < 1e-6;
  }));
  const auto [low, high] = std::minmax_element(path.begin(), path.end());
  CHECK(*high - *low > 10 * config.tick_size);
}

TEST_CASE("PriceProcess: realised volatility matches the configured one", "[price_process]") {
  PriceProcess::Config config;
  config.volatility = 0.4;
  config.tick_size = 0.0;

  const std::size_t steps = 200000;
  const std::vector<double> path = simulate(config, 2024, steps);
  double previous = config.initial_price;
  double sum_squared_returns = 0.0;
  for (double price : path) {
    const double log_return = std::log(price / previous);
    sum_squared_returns += log_return * log_return;
    previous = price;
  }
  const double years = static_cast<double>(steps) * PriceProcess::kStepSeconds /
                       config.seconds_per_year;
  // The estimator's standard error at this sample size is about 0.16%.
  CHECK_THAT(std::sqrt(sum_squared_returns / years), Catch::Matchers::WithinRel(0.4, 0.02));
}

TEST_CASE("PriceProcess: rejects an invalid configuration", "[price_process]") {
  PriceProcess::Config negative_price;
  negative_price.initial_price = -1.0;
  CHECK_THROWS_AS(PriceProcess(negative_price, 1), std::invalid_argument);

  PriceProcess::Config negative_volatility;
  negative_volatility.volatility = -0.1;
  CHECK_THROWS_AS(PriceProcess(negative_volatility, 1), std::invalid_argument);
}
