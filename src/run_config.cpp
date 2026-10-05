#include "mmsim/run_config.h"

#include <cmath>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "mmsim/avellaneda_stoikov_mm.h"
#include "mmsim/fill_model.h"
#include "mmsim/inventory_skew_mm.h"
#include "mmsim/price_process.h"
#include "mmsim/symmetric_mm.h"

namespace mmsim {

namespace {

// The numeric flags. One table drives parsing, the help text and sweep's
// --param, so a parameter added here is available in all three.
struct Parameter {
  const char* name;
  double RunConfig::* field;
  const char* help;
};

const Parameter kParameters[] = {
    {"horizon", &RunConfig::horizon, "run length in seconds"},
    {"vol", &RunConfig::volatility, "annualised volatility of the mid"},
    {"spread", &RunConfig::spread, "full bid-ask width (symmetric, skew)"},
    {"skew", &RunConfig::skew, "quote shift per unit of inventory (skew)"},
    {"gamma", &RunConfig::gamma, "risk aversion (as)"},
    {"k", &RunConfig::decay, "fill-intensity decay per price unit"},
    {"A", &RunConfig::base_intensity, "fills per second, per side, for a quote at the mid"},
};

// Whether a run of `strategy` depends on the parameter at all.
bool reads(const std::string& strategy, const std::string& parameter) {
  if (parameter == "spread") {
    return strategy != "as";
  }
  if (parameter == "skew") {
    return strategy == "skew";
  }
  if (parameter == "gamma") {
    return strategy == "as";
  }
  return true;
}

PriceProcess::Config price_config(const RunConfig& config) {
  PriceProcess::Config price;
  price.volatility = config.volatility;
  return price;
}

}  // namespace

std::vector<std::string> run_flag_names() {
  std::vector<std::string> names = {"strategy", "seed"};
  for (const Parameter& parameter : kParameters) {
    names.push_back(parameter.name);
  }
  return names;
}

std::string run_flags_help() {
  const RunConfig defaults;
  std::ostringstream help;
  help << std::left;
  help << "  --" << std::setw(9) << "strategy" << " symmetric, skew or as (default "
       << defaults.strategy << ")\n";
  for (const Parameter& parameter : kParameters) {
    help << "  --" << std::setw(9) << parameter.name << " " << parameter.help << " (default "
         << defaults.*parameter.field << ")\n";
  }
  help << "  --" << std::setw(9) << "seed" << " random seed (default " << defaults.seed << ")\n";
  return help.str();
}

RunConfig run_config_from_flags(const Flags& flags) {
  RunConfig config;
  config.strategy = flags.text("strategy", config.strategy);
  config.seed = flags.integer("seed", config.seed);
  for (const Parameter& parameter : kParameters) {
    config.*parameter.field = flags.number(parameter.name, config.*parameter.field);
  }
  return config;
}

void set_parameter(RunConfig& config, const std::string& name, double value) {
  for (const Parameter& parameter : kParameters) {
    if (name == parameter.name) {
      if (!reads(config.strategy, name)) {
        throw std::invalid_argument("parameter '" + name + "' has no effect on strategy '" +
                                    config.strategy + "'");
      }
      config.*parameter.field = value;
      return;
    }
  }
  throw std::invalid_argument("unknown parameter '" + name + "'");
}

std::unique_ptr<MarketMaker> make_maker(const RunConfig& config) {
  constexpr int kSize = 1;
  if (config.strategy == "symmetric") {
    return std::make_unique<SymmetricMM>(0.5 * config.spread, kSize);
  }
  if (config.strategy == "skew") {
    return std::make_unique<InventorySkewMM>(0.5 * config.spread, config.skew, kSize);
  }
  if (config.strategy == "as") {
    const PriceProcess::Config price = price_config(config);
    AvellanedaStoikovMM::Params params;
    params.gamma = config.gamma;
    // Relative and annualised to absolute and per sqrt(second), at the initial
    // price: over one run the mid moves too little for its level to matter.
    params.sigma = price.initial_price * price.volatility / std::sqrt(price.seconds_per_year);
    params.k = config.decay;
    params.horizon = config.horizon;
    params.size = kSize;
    return std::make_unique<AvellanedaStoikovMM>(params);
  }
  throw std::invalid_argument("unknown strategy '" + config.strategy +
                              "' (expected symmetric, skew or as)");
}

std::vector<StepRecord> simulate(const RunConfig& config) {
  if (!(config.horizon > 0.0) || std::isinf(config.horizon)) {
    throw std::invalid_argument("horizon must be positive and finite");
  }
  const auto steps =
      static_cast<std::size_t>(std::llround(config.horizon / PriceProcess::kStepSeconds));
  const Simulator simulator(price_config(config),
                            FillModel(config.base_intensity, config.decay), config.seed);
  return simulator.run(*make_maker(config), steps);
}

}  // namespace mmsim
