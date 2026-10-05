// FillModel decides how often the market trades against a resting quote. Market
// orders arrive on each side as a Poisson process whose rate decays
// exponentially with the quote's distance from the mid, lambda(d) = A *
// exp(-k * d): the further away the quote, the rarer a taker willing to reach
// it. The formula is applied as written for negative distances too, so a quote
// pushed through the mid fills faster than A and at a worse-than-mid price,
// which is how over-aggressive skew shows up as a cost instead of being hidden
// by a clamp. The per-step fill probability is the exact Poisson probability of
// at least one arrival, 1 - exp(-lambda * dt), not the lambda * dt
// approximation, so it stays a valid probability however large lambda gets.
#ifndef MMSIM_FILL_MODEL_H
#define MMSIM_FILL_MODEL_H

namespace mmsim {

class FillModel {
 public:
  // base_intensity is A, arrivals per second at distance 0; decay is k, per price unit.
  FillModel(double base_intensity, double decay);

  // lambda(d) in arrivals per second, for a quote `distance` away from the mid.
  double intensity(double distance) const;

  // Probability of at least one arrival within dt seconds: 1 - exp(-lambda * dt).
  double fill_probability(double distance, double dt) const;

 private:
  double base_intensity_;
  double decay_;
};

}  // namespace mmsim

#endif  // MMSIM_FILL_MODEL_H
