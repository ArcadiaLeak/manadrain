#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <ranges>

#include "language.hpp"

int main(int argc, char *argv[]) {
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

  Manadrain::Parser parser{};
  std::unique_ptr text_buffer{std::make_unique<std::vector<std::uint8_t>>(
      std::from_range, std::ranges::istream_view<std::uint8_t>{file})};
  parser.text_buffer = std::move(text_buffer);
  parser.parse_text();

  Manadrain::Script script{std::move(parser)};
  script.evaluate();

  return 0;
}
