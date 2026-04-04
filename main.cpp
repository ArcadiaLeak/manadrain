#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <ranges>

#include "manadrain.hpp"

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::println(std::cerr, "Usage: {} <filepath>", argv[0]);
    return 1;
  }

  std::string filepath = argv[1];
  if (not std::filesystem::exists(filepath)) {
    std::println(std::cerr, "Error: file does not exist: {}", filepath);
    return 1;
  }

  std::ifstream file{filepath, std::ios::binary};
  if (not file.is_open()) {
    std::println(std::cerr, "Error: could not open file: {}", filepath);
    return 1;
  }
  file >> std::noskipws;

  Manadrain::ParseDriver parse_buffer{
      .buffer = std::ranges::istream_view<std::uint8_t>{file} |
                std::ranges::to<std::basic_string<std::uint8_t>>()};

  return 0;
}
