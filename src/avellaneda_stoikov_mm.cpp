#include "mmsim/avellaneda_stoikov_mm.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mmsim {

AvellanedaStoikovMM::AvellanedaStoikovMM(const Params& params) : params_(params) {
  if (!(params.gamma > 0.0) || !(params.k > 0.0)) {
    throw std::invalid_argument("AvellanedaStoikovMM: gamma and k must be > 0");
  }
  if (!(params.sigma >= 0.0) || !(params.horizon >= 0.0) || params.size < 0) {
    throw std::invalid_argument("AvellanedaStoikovMM: sigma, horizon and size must be >= 0");
  }
}

double AvellanedaStoikovMM::risk_term(double time) const {
  const double time_left = std::max(params_.horizon - time, 0.0);
  return params_.gamma * params_.sigma * params_.sigma * time_left;
}

double AvellanedaStoikovMM::reservation_price(double mid, int inventory, double time) const {
  return mid - inventory * risk_term(time);
}

double AvellanedaStoikovMM::spread(double time) const {
  // log1p keeps the liquidity term accurate as gamma -> 0, where it tends to 2 / k.
  const double liquidity_term = 2.0 / params_.gamma * std::log1p(params_.gamma / params_.k);
  return risk_term(time) + liquidity_term;
}

Quotes AvellanedaStoikovMM::quote(double mid, int inventory, double time) const {
  const double reservation = reservation_price(mid, inventory, time);
  const double half_spread = 0.5 * spread(time);
  return {{reservation - half_spread, params_.size}, {reservation + half_spread, params_.size}};
}

}  // namespace mmsim
