// run_sim simulates one strategy on one seed and writes the whole step log, one
// row per simulated millisecond with that step's fills flagged, so the run can
// be plotted and every statistic recomputed from the file. It prints the run's
// summary statistics to stdout.
#include <exception>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "mmsim/csv.h"
#include "mmsim/flags.h"
#include "mmsim/run_config.h"
#include "mmsim/simulator.h"
#include "mmsim/stats.h"

namespace {

const char kUsage[] =
    "usage: run_sim [--flag value ...]\n"
    "Simulates one market-making run and writes its step log as CSV.\n"
    "  --out       output file (default path.csv)\n";

void write_path(const std::vector<mmsim::StepRecord>& log, const std::string& path) {
  std::ofstream out = mmsim::open_csv(path);
  out << "time,mid,bid,ask,inventory,pnl,bid_fill,ask_fill\n";
  for (const mmsim::StepRecord& record : log) {
    out << record.time << ',' << record.mid << ',' << record.bid << ',' << record.ask << ','
        << record.inventory << ',' << record.pnl << ',' << static_cast<int>(record.bid_filled)
        << ',' << static_cast<int>(record.ask_filled) << '\n';
  }
  out.flush();
  if (!out) {
    throw std::runtime_error("failed while writing '" + path + "'");
  }
}

}  // namespace

int main(int argc, char** argv) {
  try {
    std::vector<std::string> known = mmsim::run_flag_names();
    known.push_back("out");
    const mmsim::Flags flags(argc, argv, known);
    if (flags.help()) {
      std::cout << kUsage << mmsim::run_flags_help();
      return 0;
    }
    const mmsim::RunConfig config = mmsim::run_config_from_flags(flags);
    const std::string out = flags.text("out", "path.csv");

    const std::vector<mmsim::StepRecord> log = mmsim::simulate(config);
    write_path(log, out);

    const mmsim::RunStats stats = mmsim::summarize(log);
    std::cout << "strategy           " << config.strategy << '\n'
              << "seed               " << config.seed << '\n'
              << "steps              " << log.size() << '\n'
              << "final P&L          " << stats.final_pnl << '\n'
              << "max |inventory|    " << stats.max_abs_inventory << '\n'
              << "fills              " << stats.fill_count << '\n'
              << "max drawdown       " << stats.max_drawdown << '\n'
              << "Sharpe per step    " << stats.sharpe << '\n'
              << "wrote " << out << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "run_sim: " << error.what() << " (--help lists the flags)\n";
    return 1;
  }
}
