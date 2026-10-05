#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "mmsim/avellaneda_stoikov_mm.h"

using Catch::Matchers::WithinAbs;
using mmsim::AvellanedaStoikovMM;

namespace {

constexpr double kMid = 100.0;
constexpr double kHorizon = 1.0;
constexpr double kDecay = 1.5;

// The parameters of the paper's own simulation: sigma = 2, k = 1.5, T = 1.
AvellanedaStoikovMM make_maker(double gamma) {
  AvellanedaStoikovMM::Params params;
  params.gamma = gamma;
  params.sigma = 2.0;
  params.k = kDecay;
  params.horizon = kHorizon;
  return AvellanedaStoikovMM(params);
}

}  // namespace

TEST_CASE("AvellanedaStoikovMM: with zero inventory the reservation price equals mid",
          "[avellaneda_stoikov]") {
  const AvellanedaStoikovMM maker = make_maker(0.1);
  CHECK(maker.reservation_price(kMid, 0, 0.0) == kMid);
  CHECK(maker.reservation_price(kMid, 0, 0.7) == kMid);
}

TEST_CASE("AvellanedaStoikovMM: reservation price moves against inventory",
          "[avellaneda_stoikov]") {
  const AvellanedaStoikovMM maker = make_maker(0.1);
  // q * gamma * sigma^2 * (T - t) = 3 * 0.1 * 4 * 1 = 1.2
  CHECK_THAT(maker.reservation_price(kMid, 3, 0.0), WithinAbs(98.8, 1e-9));
  CHECK_THAT(maker.reservation_price(kMid, -3, 0.0), WithinAbs(101.2, 1e-9));
  // Half the time left, half the shift.
  CHECK_THAT(maker.reservation_price(kMid, 3, 0.5), WithinAbs(99.4, 1e-9));
}

TEST_CASE("AvellanedaStoikovMM: spread matches the closed form", "[avellaneda_stoikov]") {
  // 0.1 * 4 * 1 + (2 / 0.1) * ln(1 + 0.1 / 1.5) = 0.4 + 20 * ln(16 / 15)
  CHECK_THAT(make_maker(0.1).spread(0.0), WithinAbs(1.6907704, 1e-6));
}

TEST_CASE("AvellanedaStoikovMM: higher gamma gives a wider spread", "[avellaneda_stoikov]") {
  // True for every gamma when sigma^2 (T - t) >= 1 / k^2; here that is 4 against 0.44.
  double previous = 0.0;
  for (double gamma : {0.01, 0.1, 0.5, 1.0, 2.0}) {
    const double spread = make_maker(gamma).spread(0.0);
    CHECK(spread > previous);
    previous = spread;
  }
}

TEST_CASE("AvellanedaStoikovMM: at the horizon only the liquidity term is left",
          "[avellaneda_stoikov]") {
  // spread(T) = (2 / gamma) * ln(1 + gamma / k). It sits below 2 / k and, with no
  // time left for risk to matter, a more risk-averse maker quotes tighter, not wider.
  CHECK_THAT(make_maker(0.1).spread(kHorizon), WithinAbs(1.2907704, 1e-6));
  CHECK(make_maker(0.1).spread(kHorizon) < 2.0 / kDecay);
  CHECK(make_maker(1.0).spread(kHorizon) < make_maker(0.1).spread(kHorizon));
  // Past the horizon the time left is held at zero rather than going negative.
  CHECK(make_maker(0.1).spread(kHorizon + 5.0) == make_maker(0.1).spread(kHorizon));
  CHECK(make_maker(0.1).reservation_price(kMid, 3, kHorizon + 5.0) == kMid);
}

TEST_CASE("AvellanedaStoikovMM: quotes straddle the reservation price", "[avellaneda_stoikov]") {
  const AvellanedaStoikovMM maker = make_maker(0.1);
  const mmsim::Quotes quotes = maker.quote(kMid, 3, 0.25);

  const double centre = 0.5 * (quotes.bid.price + quotes.ask.price);
  CHECK_THAT(centre, WithinAbs(maker.reservation_price(kMid, 3, 0.25), 1e-9));
  CHECK_THAT(quotes.ask.price - quotes.bid.price, WithinAbs(maker.spread(0.25), 1e-9));
  CHECK(quotes.bid.size == 1);
  CHECK(quotes.ask.size == 1);
}

TEST_CASE("AvellanedaStoikovMM: rejects non-positive gamma or k", "[avellaneda_stoikov]") {
  CHECK_THROWS_AS(make_maker(0.0), std::invalid_argument);
  CHECK_THROWS_AS(make_maker(-0.5), std::invalid_argument);

  AvellanedaStoikovMM::Params no_decay;
  no_decay.gamma = 0.1;
  CHECK_THROWS_AS(AvellanedaStoikovMM(no_decay), std::invalid_argument);
}
