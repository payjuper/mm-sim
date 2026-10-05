#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "mmsim/flags.h"

using mmsim::Flags;

namespace {

// Parses the arguments as if they followed the program name on a command line.
Flags parse(std::vector<const char*> arguments) {
  arguments.insert(arguments.begin(), "program");
  return Flags(static_cast<int>(arguments.size()), arguments.data(), {"strategy", "vol", "seed"},
               {"log"});
}

}  // namespace

TEST_CASE("Flags: reads each value in the type asked for", "[flags]") {
  const Flags flags = parse({"--strategy", "skew", "--vol", "0.25", "--seed", "7"});
  CHECK(flags.text("strategy", "symmetric") == "skew");
  CHECK(flags.number("vol", 0.3) == 0.25);
  CHECK(flags.integer("seed", 1) == 7u);
}

TEST_CASE("Flags: an absent flag gives the fallback", "[flags]") {
  const Flags flags = parse({"--vol", "0.25"});
  CHECK(flags.has("vol"));
  CHECK_FALSE(flags.has("seed"));
  CHECK(flags.text("strategy", "symmetric") == "symmetric");
  CHECK(flags.integer("seed", 42) == 42u);
  CHECK(parse({}).number("vol", 0.3) == 0.3);
}

TEST_CASE("Flags: a value may start with a minus sign", "[flags]") {
  CHECK(parse({"--vol", "-0.5"}).number("vol", 0.0) == -0.5);
}

TEST_CASE("Flags: rejects unknown flags, missing values and stray arguments", "[flags]") {
  CHECK_THROWS_AS(parse({"--vola", "0.3"}), std::invalid_argument);
  CHECK_THROWS_AS(parse({"--vol"}), std::invalid_argument);
  CHECK_THROWS_AS(parse({"0.3"}), std::invalid_argument);
  CHECK_THROWS_AS(parse({"--vol", "0.3", "0.4"}), std::invalid_argument);
}

TEST_CASE("Flags: rejects a value that is not of the requested type", "[flags]") {
  CHECK_THROWS_AS(parse({"--vol", "0.3x"}).number("vol", 0.0), std::invalid_argument);
  CHECK_THROWS_AS(parse({"--vol", ""}).number("vol", 0.0), std::invalid_argument);
  CHECK_THROWS_AS(parse({"--seed", "-1"}).integer("seed", 0), std::invalid_argument);
  CHECK_THROWS_AS(parse({"--seed", "1.5"}).integer("seed", 0), std::invalid_argument);
  CHECK_THROWS_AS(parse({"--seed", ""}).integer("seed", 0), std::invalid_argument);
}

TEST_CASE("Flags: a switch is present or absent and takes no value", "[flags]") {
  CHECK(parse({"--log"}).has("log"));
  CHECK_FALSE(parse({"--vol", "0.3"}).has("log"));

  // The argument after a switch is the next flag, not the switch's value.
  const Flags flags = parse({"--log", "--vol", "0.25"});
  CHECK(flags.has("log"));
  CHECK(flags.number("vol", 0.3) == 0.25);
  CHECK_THROWS_AS(parse({"--log", "5"}), std::invalid_argument);
}

TEST_CASE("Flags: --help is recognised and takes no value", "[flags]") {
  CHECK(parse({"--help"}).help());
  CHECK(parse({"--vol", "0.3", "-h"}).help());
  CHECK_FALSE(parse({"--vol", "0.3"}).help());
}
