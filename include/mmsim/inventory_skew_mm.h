// InventorySkewMM keeps the symmetric maker's fixed spread but moves the centre
// of its quotes against its position: both quotes shift by -skew * inventory.
// When long, the ask sits closer to the mid and the bid further away, so sells
// become more likely than buys and inventory is pulled back towards zero; when
// short the reverse. The spread width never changes, so skew is the single knob
// that trades spread income for inventory control, which makes it the natural
// parameter to sweep. With skew = 0 it is exactly SymmetricMM. The shift is not
// capped: a large enough position pushes a quote through the mid, which is the
// strategy paying to get flat, and the simulator prices that honestly.
#ifndef MMSIM_INVENTORY_SKEW_MM_H
#define MMSIM_INVENTORY_SKEW_MM_H

#include "mmsim/market_maker.h"

namespace mmsim {

class InventorySkewMM final : public MarketMaker {
 public:
  // skew is in price units per unit of inventory.
  InventorySkewMM(double half_spread, double skew, int size);

  Quotes quote(double mid, int inventory, double time) const override;

 private:
  double half_spread_;
  double skew_;
  int size_;
};

}  // namespace mmsim

#endif  // MMSIM_INVENTORY_SKEW_MM_H
