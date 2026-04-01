#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <ranges>
#include <string>

namespace Manadrain {
void Parse(std::shared_ptr<char[]> src_buffer);
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

  uintmax_t filesize = std::filesystem::file_size(filepath);
  std::shared_ptr buffer = std::make_shared<char[]>(filesize + 1);
  file.read(buffer.get(), filesize);

  Manadrain::Parse(buffer);

  return 0;
}
