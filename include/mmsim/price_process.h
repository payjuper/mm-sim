// PriceProcess simulates the mid-price the market maker quotes around. It is a
// geometric Brownian motion advanced one simulated millisecond per step() with
// the exact log-normal update, so the path has the configured volatility however
// small the step is. Tick size is applied on output only: the process keeps an
// unrounded latent price and mid() snaps it to the grid, because rounding the
// state itself would swallow the sub-tick moves of a 1 ms step and freeze the
// path. Volatility is quoted annualised, the way it usually is, and converted
// with an explicit seconds_per_year so the convention is visible and changeable.
// Each instance owns its engine, seeded by the caller, so a run is reproducible
// and independent of every other instance.
#ifndef MMSIM_PRICE_PROCESS_H
#define MMSIM_PRICE_PROCESS_H

#include <cstdint>
#include <random>

namespace mmsim {

class PriceProcess {
 public:
  // Length of one step: one simulated millisecond.
  static constexpr double kStepSeconds = 1e-3;

  struct Config {
    double initial_price = 100.0;
    double volatility = 0.3;  // annualised, as a fraction of price
    double drift = 0.0;       // annualised
    double tick_size = 0.01;  // 0 leaves the mid unrounded
    double seconds_per_year = 252.0 * 6.5 * 3600.0;  // trading seconds
  };

  PriceProcess(const Config& config, std::uint64_t seed);

  // Current mid, rounded to the tick grid.
  double mid() const;

  // Advances one millisecond and returns the new mid.
  double step();

 private:
  double latent_price_;
  double tick_size_;
  double drift_per_step_ = 0.0;
  double vol_per_step_ = 0.0;
  std::mt19937_64 rng_;
};

}  // namespace mmsim

#endif  // MMSIM_PRICE_PROCESS_H
