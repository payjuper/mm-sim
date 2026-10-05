// RunConfig describes one simulated run in the terms a person types on the
// command line: which strategy, for how long, in what market, on which seed. It
// is the single place where those flags are named, defaulted and turned into
// the library's objects, so run_sim and sweep cannot drift apart: a flag means
// the same thing and has the same default in both, and any row of
// sweep_seeds.csv can be replayed with run_sim by passing the same values. The
// unit conversions live here too: --spread is the full bid-ask width while the
// strategies take a half-spread, and --vol is annualised and relative while
// Avellaneda-Stoikov wants price units per sqrt(second). The defaults describe
// a five-minute run whose fixed spread equals 2 / k, the width that maximises
// expected spread income under the fill model and also the limit of the
// Avellaneda-Stoikov spread as gamma -> 0, so with default flags the three
// strategies differ only in how they respond to inventory.
#ifndef MMSIM_RUN_CONFIG_H
#define MMSIM_RUN_CONFIG_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "mmsim/flags.h"
#include "mmsim/market_maker.h"
#include "mmsim/simulator.h"

namespace mmsim {

struct RunConfig {
  std::string strategy = "symmetric";  // symmetric, skew or as
  double horizon = 300.0;              // seconds
  double volatility = 0.3;             // --vol, annualised, as a fraction of price
  double spread = 0.10;                // full bid-ask width; symmetric and skew
  double skew = 0.01;                  // price units per unit of inventory; skew
  double gamma = 0.5;                  // risk aversion; as
  double decay = 20.0;                 // --k, fill-intensity decay per price unit
  double base_intensity = 5.0;         // --A, fills per second per side, quoting at the mid
  std::uint64_t seed = 1;
};

// The flags run_config_from_flags reads, for a program to pass to Flags along
// with its own.
std::vector<std::string> run_flag_names();

// One line of help per run flag, with its default.
std::string run_flags_help();

// Flags that were not given keep the defaults above.
RunConfig run_config_from_flags(const Flags& flags);

// Sets a numeric parameter by its flag name, as sweep does at each grid point.
// Throws std::invalid_argument if the name is unknown or the configured
// strategy never reads that parameter, which would make a sweep of it flat.
void set_parameter(RunConfig& config, const std::string& name, double value);

std::unique_ptr<MarketMaker> make_maker(const RunConfig& config);

// Builds the market and the maker and runs them for the horizon.
std::vector<StepRecord> simulate(const RunConfig& config);

}  // namespace mmsim

#endif  // MMSIM_RUN_CONFIG_H
