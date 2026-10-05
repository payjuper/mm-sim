#include "mmsim/csv.h"

#include <stdexcept>

namespace mmsim {

std::ofstream open_csv(const std::string& path) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    throw std::runtime_error("cannot open '" + path + "' for writing");
  }
  out.precision(10);
  return out;
}

}  // namespace mmsim
