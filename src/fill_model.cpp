#include "mmsim/fill_model.h"

#include <cmath>
#include <stdexcept>

namespace mmsim {

FillModel::FillModel(double base_intensity, double decay)
    : base_intensity_(base_intensity), decay_(decay) {
  if (!(base_intensity >= 0.0) || !(decay > 0.0)) {
    throw std::invalid_argument("FillModel: base_intensity must be >= 0 and decay > 0");
  }
}

double FillModel::intensity(double distance) const {
  return base_intensity_ * std::exp(-decay_ * distance);
}

double FillModel::fill_probability(double distance, double dt) const {
  // -expm1(-x) is 1 - exp(-x) without the rounding loss at small x; it saturates at 1.
  return -std::expm1(-intensity(distance) * dt);
}

}  // namespace mmsim
