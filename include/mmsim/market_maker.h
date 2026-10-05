// MarketMaker is the seam between a quoting rule and the simulator. A strategy
// is asked one question per step: given the mid, its signed inventory and the
// clock, where does it quote? It answers with both sides at once because the
// two quotes are always chosen together. quote() is const and receives
// everything it needs as arguments, so a strategy holds parameters but no
// evolving state: the simulator alone owns inventory and cash, one maker can be
// reused across seeds, and a test can probe a strategy with plain calls.
#ifndef MMSIM_MARKET_MAKER_H
#define MMSIM_MARKET_MAKER_H

namespace mmsim {

// One side of the maker's quote. Size is in whole units; 0 pulls that side.
struct Quote {
  double price;
  int size;
};

struct Quotes {
  Quote bid;
  Quote ask;
};

class MarketMaker {
 public:
  virtual ~MarketMaker();

  // mid: current mid-price. inventory: signed units held, positive when long.
  // time: seconds since the start of the run.
  virtual Quotes quote(double mid, int inventory, double time) const = 0;
};

}  // namespace mmsim

#endif  // MMSIM_MARKET_MAKER_H
