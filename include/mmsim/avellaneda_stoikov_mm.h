// AvellanedaStoikovMM quotes from the closed-form solution in Avellaneda and
// Stoikov (2008), "High-frequency trading in a limit order book". The maker
// values its position at a reservation price r = mid - q * gamma * sigma^2 *
// (T - t), which drops below the mid when long, and quotes r -/+ half of the
// optimal spread gamma * sigma^2 * (T - t) + (2 / gamma) * ln(1 + gamma / k).
// Unlike the fixed skew rule, both the shift and the width scale with risk
// (sigma), risk aversion (gamma) and the time left, so inventory control is
// strong early and fades to nothing at the horizon. The two formulas are public
// methods so tests and analyses can read them directly. sigma is the mid's
// volatility in price units per sqrt(second), as in the paper's arithmetic
// model, and k must match the fill model's decay rate for the spread to be
// optimal.
#ifndef MMSIM_AVELLANEDA_STOIKOV_MM_H
#define MMSIM_AVELLANEDA_STOIKOV_MM_H

#include "mmsim/market_maker.h"

namespace mmsim {

class AvellanedaStoikovMM final : public MarketMaker {
 public:
  struct Params {
    double gamma = 0.0;    // risk aversion, must be > 0
    double sigma = 0.0;    // mid volatility, price units per sqrt(second)
    double k = 0.0;        // fill-intensity decay per price unit, must be > 0
    double horizon = 0.0;  // T, in seconds
    int size = 1;
  };

  explicit AvellanedaStoikovMM(const Params& params);

  double reservation_price(double mid, int inventory, double time) const;

  // Total bid-ask spread at the given time.
  double spread(double time) const;

  Quotes quote(double mid, int inventory, double time) const override;

 private:
  // gamma * sigma^2 * (T - t), with the time left floored at zero.
  double risk_term(double time) const;

  Params params_;
};

}  // namespace mmsim

#endif  // MMSIM_AVELLANEDA_STOIKOV_MM_H
