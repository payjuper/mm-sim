#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "mmsim/avellaneda_stoikov_mm.h"
#include "mmsim/inventory_skew_mm.h"
#include "mmsim/market_maker.h"
#include "mmsim/symmetric_mm.h"

using Catch::Matchers::WithinAbs;
using mmsim::InventorySkewMM;
using mmsim::Quotes;
using mmsim::SymmetricMM;

namespace {

constexpr double kMid = 100.0;
constexpr double kHalfSpread = 0.05;
constexpr double kTolerance = 1e-12;

}  // namespace

TEST_CASE("SymmetricMM: quotes are symmetric around mid", "[market_maker]") {
  const SymmetricMM maker(kHalfSpread, 3);
  const Quotes quotes = maker.quote(kMid, 0, 0.0);

  CHECK_THAT(kMid - quotes.bid.price, WithinAbs(kHalfSpread, kTolerance));
  CHECK_THAT(quotes.ask.price - kMid, WithinAbs(kHalfSpread, kTolerance));
  CHECK(quotes.bid.size == 3);
  CHECK(quotes.ask.size == 3);
}

TEST_CASE("SymmetricMM: quotes ignore inventory and time", "[market_maker]") {
  const SymmetricMM maker(kHalfSpread, 1);
  const Quotes flat = maker.quote(kMid, 0, 0.0);
  const Quotes loaded = maker.quote(kMid, 25, 123.0);

  CHECK(loaded.bid.price == flat.bid.price);
  CHECK(loaded.ask.price == flat.ask.price);
}

TEST_CASE("InventorySkewMM: long inventory puts both quotes below the symmetric ones",
          "[market_maker]") {
  const SymmetricMM symmetric(kHalfSpread, 1);
  const InventorySkewMM skewed(kHalfSpread, 0.01, 1);
  const Quotes base = symmetric.quote(kMid, 4, 0.0);
  const Quotes quotes = skewed.quote(kMid, 4, 0.0);

  CHECK(quotes.bid.price < base.bid.price);
  CHECK(quotes.ask.price < base.ask.price);
  CHECK_THAT(base.bid.price - quotes.bid.price, WithinAbs(0.04, kTolerance));
  CHECK_THAT(base.ask.price - quotes.ask.price, WithinAbs(0.04, kTolerance));
}

TEST_CASE("InventorySkewMM: short inventory puts both quotes above the symmetric ones",
          "[market_maker]") {
  const SymmetricMM symmetric(kHalfSpread, 1);
  const InventorySkewMM skewed(kHalfSpread, 0.01, 1);
  const Quotes base = symmetric.quote(kMid, -4, 0.0);
  const Quotes quotes = skewed.quote(kMid, -4, 0.0);

  CHECK(quotes.bid.price > base.bid.price);
  CHECK(quotes.ask.price > base.ask.price);
}

TEST_CASE("InventorySkewMM: matches SymmetricMM when flat or when skew is zero",
          "[market_maker]") {
  const Quotes base = SymmetricMM(kHalfSpread, 1).quote(kMid, 0, 0.0);
  const Quotes flat = InventorySkewMM(kHalfSpread, 0.01, 1).quote(kMid, 0, 0.0);
  const Quotes unskewed = InventorySkewMM(kHalfSpread, 0.0, 1).quote(kMid, 9, 0.0);

  CHECK(flat.bid.price == base.bid.price);
  CHECK(flat.ask.price == base.ask.price);
  CHECK(unskewed.bid.price == base.bid.price);
  CHECK(unskewed.ask.price == base.ask.price);
}

TEST_CASE("InventorySkewMM: skew moves the quotes but not the spread width", "[market_maker]") {
  const Quotes quotes = InventorySkewMM(kHalfSpread, 0.01, 1).quote(kMid, 7, 0.0);
  CHECK_THAT(quotes.ask.price - quotes.bid.price, WithinAbs(2 * kHalfSpread, kTolerance));
}

TEST_CASE("Strategies reject invalid parameters", "[market_maker]") {
  CHECK_THROWS_AS(SymmetricMM(-0.01, 1), std::invalid_argument);
  CHECK_THROWS_AS(SymmetricMM(kHalfSpread, -1), std::invalid_argument);
  CHECK_THROWS_AS(InventorySkewMM(kHalfSpread, -0.01, 1), std::invalid_argument);
  CHECK_NOTHROW(SymmetricMM(std::numeric_limits<double>::infinity(), 1));
}

TEST_CASE("Every strategy quotes a bid below its ask through the interface", "[market_maker]") {
  mmsim::AvellanedaStoikovMM::Params params;
  params.gamma = 0.1;
  params.sigma = 2.0;
  params.k = 1.5;
  params.horizon = 1.0;

  std::vector<std::unique_ptr<mmsim::MarketMaker>> makers;
  makers.push_back(std::make_unique<SymmetricMM>(kHalfSpread, 1));
  makers.push_back(std::make_unique<InventorySkewMM>(kHalfSpread, 0.01, 1));
  makers.push_back(std::make_unique<mmsim::AvellanedaStoikovMM>(params));

  for (const auto& maker : makers) {
    for (int inventory : {-10, 0, 10}) {
      const Quotes quotes = maker->quote(kMid, inventory, 0.5);
      CHECK(quotes.bid.price < quotes.ask.price);
    }
  }
}
