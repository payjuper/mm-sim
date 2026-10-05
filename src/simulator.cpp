#include "mmsim/simulator.h"

#include "mmsim/random.h"

namespace mmsim {

namespace {

// The fill stream is seeded through a seed_seq with its own stream id, so it is
// unrelated to the price stream that PriceProcess seeds directly from `seed`.
std::mt19937_64 make_fill_rng(std::uint64_t seed) {
  std::seed_seq sequence{static_cast<std::uint32_t>(seed),
                         static_cast<std::uint32_t>(seed >> 32), std::uint32_t{1}};
  return std::mt19937_64(sequence);
}

}  // namespace

Simulator::Simulator(const PriceProcess::Config& price, const FillModel& fills,
                     std::uint64_t seed)
    : start_(price, seed), fills_(fills), seed_(seed) {}

std::vector<StepRecord> Simulator::run(const MarketMaker& maker, std::size_t steps) const {
  PriceProcess price = start_;
  std::mt19937_64 fill_rng = make_fill_rng(seed_);
  std::vector<StepRecord> log;
  log.reserve(steps);
  StepRecord state;  // flat and with no cash before the first step
  for (std::size_t i = 0; i < steps; ++i) {
    const double time = static_cast<double>(i) * PriceProcess::kStepSeconds;
    state = trade(maker, state, time, price.mid(), fill_rng);
    log.push_back(state);
    price.step();
  }
  return log;
}

StepRecord Simulator::trade(const MarketMaker& maker, const StepRecord& previous, double time,
                            double mid, std::mt19937_64& fill_rng) const {
  const Quotes quotes = maker.quote(mid, previous.inventory, time);
  StepRecord record = previous;
  record.time = time;
  record.mid = mid;
  record.bid = quotes.bid.price;
  record.ask = quotes.ask.price;
  record.bid_filled = order_arrives(mid - quotes.bid.price, quotes.bid.size, fill_rng);
  record.ask_filled = order_arrives(quotes.ask.price - mid, quotes.ask.size, fill_rng);
  if (record.bid_filled) {  // a seller hit the bid: the maker buys one unit
    record.inventory += 1;
    record.cash -= quotes.bid.price;
  }
  if (record.ask_filled) {  // a buyer lifted the ask: the maker sells one unit
    record.inventory -= 1;
    record.cash += quotes.ask.price;
  }
  record.pnl = record.cash + record.inventory * mid;
  return record;
}

bool Simulator::order_arrives(double distance, int size, std::mt19937_64& fill_rng) const {
  // Drawn even when the side is pulled, so every strategy consumes the same stream.
  const double draw = uniform01(fill_rng);
  return size > 0 && draw < fills_.fill_probability(distance, PriceProcess::kStepSeconds);
}

}  // namespace mmsim
