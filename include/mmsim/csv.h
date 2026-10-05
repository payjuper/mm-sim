// The one decision the output files share: how a CSV is opened. Files are
// written in binary mode so lines end in "\n" on every platform, and numbers
// carry 10 significant digits, which keeps a tick-aligned price readable
// (100.07, not 100.06999999999999) while resolving far below one tick. With
// both fixed here, run_sim and sweep print a number identically and a file does
// not change with the platform's line-ending convention.
#ifndef MMSIM_CSV_H
#define MMSIM_CSV_H

#include <fstream>
#include <string>

namespace mmsim {

// Opens `path` for writing, replacing any existing file. Throws
// std::runtime_error if it cannot be opened.
std::ofstream open_csv(const std::string& path);

}  // namespace mmsim

#endif  // MMSIM_CSV_H
