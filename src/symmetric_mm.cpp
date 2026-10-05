#include "mmsim/symmetric_mm.h"

#include <stdexcept>

namespace mmsim {

SymmetricMM::SymmetricMM(double half_spread, int size) : half_spread_(half_spread), size_(size) {
  if (!(half_spread >= 0.0) || size < 0) {
    throw std::invalid_argument("SymmetricMM: half_spread and size must be >= 0");
  }
}

Quotes SymmetricMM::quote(double mid, int /*inventory*/, double /*time*/) const {
  return {{mid - half_spread_, size_}, {mid + half_spread_, size_}};
}

}  // namespace mmsim
