#include "mmsim/price_process.h"

#include <cmath>
#include <stdexcept>

#include "mmsim/random.h"

namespace mmsim {

PriceProcess::PriceProcess(const Config& config, std::uint64_t seed)
    : latent_price_(config.initial_price), tick_size_(config.tick_size), rng_(seed) {
  // Written as negations so that NaN is rejected too.
  if (!(config.initial_price > 0.0) || !(config.seconds_per_year > 0.0)) {
    throw std::invalid_argument("PriceProcess: initial_price and seconds_per_year must be > 0");
  }
  if (!(config.volatility >= 0.0) || !(config.tick_size >= 0.0)) {
    throw std::invalid_argument("PriceProcess: volatility and tick_size must be >= 0");
  }
  // Exact GBM step: S *= exp((mu - sigma^2 / 2) dt + sigma sqrt(dt) Z).
  const double dt_years = kStepSeconds / config.seconds_per_year;
  drift_per_step_ = (config.drift - 0.5 * config.volatility * config.volatility) * dt_years;
  vol_per_step_ = config.volatility * std::sqrt(dt_years);
}

double PriceProcess::mid() const {
  if (tick_size_ == 0.0) {
    return latent_price_;
  }
  return std::round(latent_price_ / tick_size_) * tick_size_;
}

double PriceProcess::step() {
  latent_price_ *= std::exp(drift_per_step_ + vol_per_step_ * standard_normal(rng_));
  return mid();
}

}  // namespace mmsim
