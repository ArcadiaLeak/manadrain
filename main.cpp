#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <ranges>
#include <string>

namespace Manadrain {
void Parse(std::string source_str);
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

  std::ifstream file{filepath};
  if (not file.is_open())
    throw std::runtime_error{std::format("could not open file: {}", filepath)};

  std::string source_str = std::ranges::to<std::string>(std::ranges::subrange(
      std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}));

  Manadrain::Parse(std::move(source_str));

  return 0;
}
