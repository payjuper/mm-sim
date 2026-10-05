// Flags turns a command line into typed values. A flag is written
// "--name value", or bare "--name" for an on/off switch, so the parser needs no
// schema beyond the names the program accepts of each kind. It rejects anything
// outside those lists instead of ignoring it, because a misspelt flag that
// silently fell back to its default would produce a plausible run of the wrong
// experiment. Values stay text until a getter asks for them in a particular
// type, and each getter takes the value to use when the flag was not given, so
// a default is stated where it is used. "--help" (or "-h") is always accepted.
#ifndef MMSIM_FLAGS_H
#define MMSIM_FLAGS_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace mmsim {

class Flags {
 public:
  // argv[0] is skipped. `known` are the flags that take a value, `switches`
  // the ones that take none. Throws std::invalid_argument for a flag in neither
  // list, a value flag without a value, or an argument that is not a flag.
  Flags(int argc, const char* const* argv, const std::vector<std::string>& known,
        const std::vector<std::string>& switches = {});

  bool help() const;

  // Whether the flag was given; the only question to ask of a switch.
  bool has(const std::string& name) const;

  // Each getter returns `fallback` when the flag is absent and throws
  // std::invalid_argument when its value is not of the requested type.
  std::string text(const std::string& name, const std::string& fallback) const;
  double number(const std::string& name, double fallback) const;
  std::uint64_t integer(const std::string& name, std::uint64_t fallback) const;  // non-negative

 private:
  std::map<std::string, std::string> values_;  // a switch is stored with an empty value
  bool help_ = false;
};

}  // namespace mmsim

#endif  // MMSIM_FLAGS_H
