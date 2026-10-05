#include "mmsim/inventory_skew_mm.h"

#include <stdexcept>

namespace mmsim {

InventorySkewMM::InventorySkewMM(double half_spread, double skew, int size)
    : half_spread_(half_spread), skew_(skew), size_(size) {
  if (!(half_spread >= 0.0) || !(skew >= 0.0) || size < 0) {
    throw std::invalid_argument("InventorySkewMM: half_spread, skew and size must be >= 0");
  }
}

Quotes InventorySkewMM::quote(double mid, int inventory, double /*time*/) const {
  const double centre = mid - skew_ * inventory;
  return {{centre - half_spread_, size_}, {centre + half_spread_, size_}};
}

}  // namespace mmsim
