// sweep runs one strategy over a grid of values of one parameter, on the same
// set of seeds at every grid point. Because a seed fixes the price path and the
// order flow whatever the strategy does, the grid points are compared on
// identical markets and the curve across the grid is much smoother than the
// seed-to-seed noise. It writes two files: one row per grid point with the
// statistics averaged over seeds, and one row per seed per grid point for
// looking at the distribution behind each average.
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "mmsim/csv.h"
#include "mmsim/flags.h"
#include "mmsim/run_config.h"
#include "mmsim/stats.h"

namespace {

const char kUsage[] =
    "usage: sweep --param NAME --from X --to Y [--flag value ...]\n"
    "Runs one strategy over a grid of one parameter, on the same seeds at each value.\n"
    "  --param     numeric run flag to vary, e.g. skew or gamma\n"
    "  --from      first grid value\n"
    "  --to        last grid value\n"
    "  --steps     number of grid values (default 11)\n"
    "  --log       space the grid values geometrically instead of evenly; a switch,\n"
    "              takes no value, and needs --from and --to to be positive\n"
    "  --seeds     runs per grid value, on seeds --seed, --seed + 1, ... (default 50)\n"
    "  --out       per-value output file (default sweep.csv); the per-seed rows go\n"
    "              next to it with _seeds added to the name\n";

// The statistics columns, in the order both files write them.
struct Column {
  const char* name;
  double mmsim::RunStats::* field;
};

const Column kColumns[] = {
    {"final_pnl", &mmsim::RunStats::final_pnl},
    {"mean_step_pnl", &mmsim::RunStats::mean_step_pnl},
    {"std_step_pnl", &mmsim::RunStats::std_step_pnl},
    {"sharpe", &mmsim::RunStats::sharpe},
    {"max_drawdown", &mmsim::RunStats::max_drawdown},
    {"max_abs_inventory", &mmsim::RunStats::max_abs_inventory},
    {"fill_count", &mmsim::RunStats::fill_count},
};

void write_header(std::ostream& out, const char* count_column) {
  out << "strategy,param,value," << count_column;
  for (const Column& column : kColumns) {
    out << ',' << column.name;
  }
}

void write_stats(std::ostream& out, const mmsim::RunStats& stats) {
  for (const Column& column : kColumns) {
    out << ',' << stats.*column.field;
  }
}

// sweep.csv -> sweep_seeds.csv, in the same directory.
std::string seeds_path(const std::string& out) {
  const std::string extension = std::filesystem::path(out).extension().string();
  return out.substr(0, out.size() - extension.size()) + "_seeds" + extension;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    std::vector<std::string> known = mmsim::run_flag_names();
    for (const char* name : {"param", "from", "to", "steps", "seeds", "out"}) {
      known.push_back(name);
    }
    const mmsim::Flags flags(argc, argv, known, {"log"});
    if (flags.help()) {
      std::cout << kUsage << mmsim::run_flags_help();
      return 0;
    }
    if (!flags.has("param") || !flags.has("from") || !flags.has("to")) {
      throw std::invalid_argument("--param, --from and --to are required");
    }
    mmsim::RunConfig config = mmsim::run_config_from_flags(flags);
    const std::string param = flags.text("param", "");
    const double from = flags.number("from", 0.0);
    const double to = flags.number("to", 0.0);
    const bool log_spaced = flags.has("log");
    const std::uint64_t steps = flags.integer("steps", 11);
    const std::uint64_t seeds = flags.integer("seeds", 50);
    const std::uint64_t first_seed = config.seed;
    const std::string out = flags.text("out", "sweep.csv");
    if (steps < 1 || seeds < 1) {
      throw std::invalid_argument("--steps and --seeds must be at least 1");
    }
    if (log_spaced && !(from > 0.0 && to > 0.0)) {
      throw std::invalid_argument("--log needs --from and --to to be positive");
    }
    // Reject a bad parameter or strategy before any existing output is replaced.
    mmsim::set_parameter(config, param, from);
    mmsim::make_maker(config);

    std::ofstream by_value = mmsim::open_csv(out);
    std::ofstream by_seed = mmsim::open_csv(seeds_path(out));
    write_header(by_value, "seeds");
    by_value << ",std_final_pnl\n";
    write_header(by_seed, "seed");
    by_seed << '\n';

    std::cout << std::fixed << std::setw(10) << param << std::setw(12) << "final_pnl"
              << std::setw(15) << "std_final_pnl" << std::setw(19) << "max_abs_inventory"
              << std::setw(12) << "fill_count" << '\n';
    for (std::uint64_t i = 0; i < steps; ++i) {
      const double fraction =
          steps > 1 ? static_cast<double>(i) / static_cast<double>(steps - 1) : 0.0;
      const double value =
          log_spaced ? from * std::pow(to / from, fraction) : from + (to - from) * fraction;
      mmsim::set_parameter(config, param, value);

      std::vector<mmsim::RunStats> runs;
      for (std::uint64_t s = 0; s < seeds; ++s) {
        config.seed = first_seed + s;
        runs.push_back(mmsim::summarize(mmsim::simulate(config)));
        by_seed << config.strategy << ',' << param << ',' << value << ',' << config.seed;
        write_stats(by_seed, runs.back());
        by_seed << '\n';
      }
      const mmsim::AggregateStats result = mmsim::aggregate(runs);
      by_value << config.strategy << ',' << param << ',' << value << ',' << seeds;
      write_stats(by_value, result.mean);
      by_value << ',' << result.std_final_pnl << '\n';

      std::cout << std::defaultfloat << std::setprecision(4) << std::setw(10) << value
                << std::fixed << std::setw(12) << result.mean.final_pnl << std::setw(15)
                << result.std_final_pnl
                << std::setprecision(2) << std::setw(19) << result.mean.max_abs_inventory
                << std::setw(12) << result.mean.fill_count << std::endl;
    }
    by_value.flush();
    by_seed.flush();
    if (!by_value || !by_seed) {
      throw std::runtime_error("failed while writing '" + out + "' or its _seeds file");
    }
    std::cout << "wrote " << out << " and " << seeds_path(out) << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "sweep: " << error.what() << " (--help lists the flags)\n";
    return 1;
  }
}
