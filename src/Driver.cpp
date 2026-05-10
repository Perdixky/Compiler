#include <rfl.hpp>
#include <rfl/cli.hpp>
#include <mio/mmap.hpp>

import std;

struct Args {
  rfl::Positional<std::string> input_file;
  rfl::Short<"o", std::string> output_file = "a.out";
};

auto main(int argc, char** argv) -> int {
  auto config = rfl::cli::read<Args, rfl::DefaultIfMissing>(argc, argv);
  if (!config) {
    std::println("Failed to parse arguments: {}", config.error().what());
    return 1;
  }

  std::error_code err;
  mio::mmap_source input_file = mio::make_mmap_source(config.value().input_file.value(), err);
  if (err) {
    std::println("Failed to open input file: {}", err.message());
    return 1;
  }

  return 0;
}
