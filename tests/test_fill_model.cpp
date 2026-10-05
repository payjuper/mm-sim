#include <cmath>
#include <limits>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "mmsim/fill_model.h"

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using mmsim::FillModel;

namespace {

constexpr double kBaseIntensity = 5.0;  // A, per second
constexpr double kDecay = 20.0;         // k, per price unit
constexpr double kStep = 1e-3;          // one simulated millisecond

}  // namespace

TEST_CASE("FillModel: at distance 0 the intensity equals A", "[fill_model]") {
  CHECK(FillModel(kBaseIntensity, kDecay).intensity(0.0) == kBaseIntensity);
}

TEST_CASE("FillModel: intensity is monotonically decreasing in distance", "[fill_model]") {
  const FillModel model(kBaseIntensity, kDecay);
  // The range starts below zero: quotes through the mid follow the same curve, unclamped.
  double previous = model.intensity(-0.51);
  for (int ticks = -50; ticks <= 100; ++ticks) {
    const double intensity = model.intensity(ticks * 0.01);
    CHECK(intensity < previous);
    previous = intensity;
  }
  CHECK(model.intensity(-0.05) > kBaseIntensity);
  CHECK_THAT(model.intensity(0.05), WithinRel(kBaseIntensity * std::exp(-1.0), 1e-12));
}

TEST_CASE("FillModel: an infinitely distant quote is never filled", "[fill_model]") {
  const FillModel model(kBaseIntensity, kDecay);
  const double infinity = std::numeric_limits<double>::infinity();
  CHECK(model.intensity(infinity) == 0.0);
  CHECK(model.fill_probability(infinity, kStep) == 0.0);
}

TEST_CASE("FillModel: step fill probability is 1 - exp(-lambda * dt)", "[fill_model]") {
  const FillModel model(kBaseIntensity, kDecay);
  for (double distance : {-0.3, -0.05, 0.0, 0.05, 0.2}) {
    for (double dt : {1e-3, 0.1, 2.0}) {
      const double lambda = model.intensity(distance);
      CHECK_THAT(model.fill_probability(distance, dt),
                 WithinRel(1.0 - std::exp(-lambda * dt), 1e-9));
    }
  }
}

TEST_CASE("FillModel: step fill probability stays valid when lambda is large", "[fill_model]") {
  const FillModel model(kBaseIntensity, kDecay);

  // lambda * dt = 5 * exp(6) * 0.001, about 2.02: the linear approximation would
  // already exceed 1 here, while the exact probability is about 0.867.
  const double lambda_dt = model.intensity(-0.3) * kStep;
  REQUIRE(lambda_dt > 2.0);
  CHECK_THAT(model.fill_probability(-0.3, kStep), WithinAbs(1.0 - std::exp(-lambda_dt), 1e-12));
  CHECK(model.fill_probability(-0.3, kStep) < 1.0);

  // Far through the mid, and past the point where lambda itself overflows to
  // infinity, the probability saturates at exactly 1 and never goes above it.
  for (double distance : {-1.0, -5.0, -1000.0}) {
    const double probability = model.fill_probability(distance, kStep);
    CHECK(probability == 1.0);
  }
}

TEST_CASE("FillModel: for small lambda * dt the probability approaches lambda * dt",
          "[fill_model]") {
  const FillModel model(kBaseIntensity, kDecay);
  const double lambda_dt = model.intensity(0.05) * kStep;  // about 0.0018
  const double probability = model.fill_probability(0.05, kStep);
  CHECK(probability < lambda_dt);
  CHECK_THAT(probability, WithinRel(lambda_dt, 1e-3));
}

TEST_CASE("FillModel: fill probability falls with distance and rises with dt", "[fill_model]") {
  const FillModel model(kBaseIntensity, kDecay);
  CHECK(model.fill_probability(0.10, kStep) < model.fill_probability(0.05, kStep));
  CHECK(model.fill_probability(0.05, kStep) < model.fill_probability(0.05, 10 * kStep));
}

TEST_CASE("FillModel: rejects invalid parameters", "[fill_model]") {
  CHECK_THROWS_AS(FillModel(-1.0, kDecay), std::invalid_argument);
  CHECK_THROWS_AS(FillModel(kBaseIntensity, 0.0), std::invalid_argument);
  CHECK_THROWS_AS(FillModel(kBaseIntensity, -1.0), std::invalid_argument);
}
