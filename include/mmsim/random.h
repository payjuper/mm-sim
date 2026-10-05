// Random draws shared by the price process and the fill model. The engine,
// std::mt19937_64, produces the same bit stream on every platform, but the
// standard distributions (std::normal_distribution and friends) are
// implementation-defined, so the same seed gives different numbers under
// libstdc++, libc++ and MSVC. These two transforms are written out by hand so
// that a seed pins down a run regardless of which standard library built it.
// They are free functions over a caller-owned engine: nothing here holds state.
#ifndef MMSIM_RANDOM_H
#define MMSIM_RANDOM_H

#include <cmath>
#include <random>

namespace mmsim {

// Uniform on [0, 1), built from the top 53 bits of one engine output.
inline double uniform01(std::mt19937_64& rng) {
  return static_cast<double>(rng() >> 11) * 0x1.0p-53;
}

// Standard normal by Box-Muller; consumes exactly two engine outputs per draw.
inline double standard_normal(std::mt19937_64& rng) {
  constexpr double kTwoPi = 6.283185307179586476925;
  const double radius_u = 1.0 - uniform01(rng);  // (0, 1], keeps the log finite
  const double angle_u = uniform01(rng);
  return std::sqrt(-2.0 * std::log(radius_u)) * std::cos(kTwoPi * angle_u);
}

}  // namespace mmsim

#endif  // MMSIM_RANDOM_H
