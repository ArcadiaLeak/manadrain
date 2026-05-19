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
  std::vector<std::uint8_t> text_vec{
      std::from_range, std::ranges::istream_view<std::uint8_t>{file}};
  std::shared_ptr text_buffer{
      std::make_shared<std::uint8_t[]>(text_vec.size())};
  std::memcpy(text_buffer.get(), text_vec.data(), text_vec.size());
  parser.text_buffer = text_buffer;
  parser.text_size = text_vec.size();
  parser.parse_text();

  Manadrain::Script script{std::move(parser.script)};
  auto console_log =
      [](std::vector<Manadrain::DYNAMIC> arguments, Manadrain::DYNAMIC context,
         const Manadrain::Script &s) { return Manadrain::DYNAMIC{}; };
  std::size_t log_atom{script.attach_atom("log")};
  Manadrain::FUNCTION_HANDLE hdl_console_log{script.insert(console_log)};
  Manadrain::OBJECT console_obj{
      {log_atom, Manadrain::FUNCTION_HANDLE{hdl_console_log}}};
  std::size_t console_atom{script.attach_atom("console")};
  Manadrain::OBJECT_HANDLE hdl_console{script.insert(std::move(console_obj))};
  script.main_function.function_scope[console_atom] = hdl_console;
  script.execute();

  return 0;
}
