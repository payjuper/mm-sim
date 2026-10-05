#include "mmsim/flags.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <stdexcept>

namespace mmsim {

namespace {

bool contains(const std::vector<std::string>& names, const std::string& name) {
  return std::find(names.begin(), names.end(), name) != names.end();
}

}  // namespace

Flags::Flags(int argc, const char* const* argv, const std::vector<std::string>& known,
             const std::vector<std::string>& switches) {
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--help" || argument == "-h") {
      help_ = true;
      continue;
    }
    if (argument.rfind("--", 0) != 0) {
      throw std::invalid_argument("unexpected argument '" + argument + "'");
    }
    const std::string name = argument.substr(2);
    if (contains(switches, name)) {
      values_[name] = "";
      continue;
    }
    if (!contains(known, name)) {
      throw std::invalid_argument("unknown flag --" + name);
    }
    if (i + 1 == argc) {
      throw std::invalid_argument("flag --" + name + " needs a value");
    }
    // The next argument is the value whatever it looks like, so "--from -0.5" works.
    values_[name] = argv[++i];
  }
}

bool Flags::help() const { return help_; }

bool Flags::has(const std::string& name) const { return values_.count(name) > 0; }

std::string Flags::text(const std::string& name, const std::string& fallback) const {
  const auto found = values_.find(name);
  return found == values_.end() ? fallback : found->second;
}

double Flags::number(const std::string& name, double fallback) const {
  const auto found = values_.find(name);
  if (found == values_.end()) {
    return fallback;
  }
  const std::string& text = found->second;
  char* end = nullptr;
  const double value = std::strtod(text.c_str(), &end);
  if (text.empty() || *end != '\0') {
    throw std::invalid_argument("--" + name + ": '" + text + "' is not a number");
  }
  return value;
}

std::uint64_t Flags::integer(const std::string& name, std::uint64_t fallback) const {
  const auto found = values_.find(name);
  if (found == values_.end()) {
    return fallback;
  }
  const std::string& text = found->second;
  const char* const end = text.data() + text.size();
  std::uint64_t value = 0;
  const std::from_chars_result result = std::from_chars(text.data(), end, value);
  if (result.ec != std::errc() || result.ptr != end) {
    throw std::invalid_argument("--" + name + ": '" + text + "' is not a non-negative integer");
  }
  return value;
}

}  // namespace mmsim
