// SymmetricMM is the baseline strategy: it quotes mid - half_spread and
// mid + half_spread with a fixed size and ignores both inventory and time. It
// has no risk control at all, which is the point: its inventory is a pure
// random walk driven by the fill flow, so it shows what the other strategies
// are buying when they give up spread income to manage inventory. An infinite
// half-spread is allowed and means "never trade", which is handy in tests.
#ifndef MMSIM_SYMMETRIC_MM_H
#define MMSIM_SYMMETRIC_MM_H

#include "mmsim/market_maker.h"

namespace mmsim {

class SymmetricMM final : public MarketMaker {
 public:
  SymmetricMM(double half_spread, int size);

  Quotes quote(double mid, int inventory, double time) const override;

 private:
  double half_spread_;
  int size_;
};

}  // namespace mmsim

#endif  // MMSIM_SYMMETRIC_MM_H
