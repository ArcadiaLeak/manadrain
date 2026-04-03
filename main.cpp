#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <ranges>
#include <string>

namespace Manadrain {
void Parse(const std::string& src_string);
}

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
  if (not file.is_open())
    throw std::runtime_error{std::format("could not open file: {}", filepath)};
  file >> std::noskipws;

  const std::string src_string =
      std::ranges::istream_view<char>{file} | std::ranges::to<std::string>();
  Manadrain::Parse(src_string);

  return 0;
}
