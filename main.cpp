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

  Manadrain::Script script{};
  std::vector<std::uint8_t> text_source{
      std::from_range, std::ranges::istream_view<std::uint8_t>{file}};
  script.parse_text(std::move(text_source));

  auto console_log = [](std::vector<Manadrain::DYNAMIC> parameter_vec,
                        Manadrain::DYNAMIC context,
                        const Manadrain::Script &s) {
    return Manadrain::DYNAMIC{};
  };
  Manadrain::FUNCTION_HANDLE hdl_console_log{
      script.insert_function(console_log)};
  std::size_t log_atom{script.insert_atom("log")};
  Manadrain::OBJECT console_obj{
      {log_atom, Manadrain::FUNCTION_HANDLE{hdl_console_log}}};
  Manadrain::OBJECT_HANDLE hdl_console{
      script.insert_object(std::move(console_obj))};
  std::size_t console_atom{script.insert_atom("console")};
  script.pin_global(console_atom, hdl_console);
  script.execute();

  return 0;
}
