#include <cassert>
#include <filesystem>
#include <fstream>
#include <inplace_vector>
#include <iostream>
#include <print>
#include <ranges>
#include <thread>

#include <unistr.h>

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

  auto convert_uchar = [](std::uint16_t uchar) {
    std::array<std::uint8_t, 3> buffer{};
    std::size_t buffer_len{buffer.size()};
    uint8_t *result = u16_to_u8(&uchar, 1, buffer.data(), &buffer_len);
    assert(result != nullptr);
    auto u8_encoded = buffer | std::views::take(buffer_len);
    return std::inplace_vector<std::uint8_t, 3>{std::from_range, u8_encoded};
  };
  auto convert_message = [&](const std::u16string &message) {
    auto u8_message =
        message | std::views::transform(convert_uchar) | std::views::join;
    return std::string{std::from_range, u8_message};
  };
  auto console_printer = [&](std::stop_token stopper) {
    std::unique_lock console_lock{*script.console_mutex};
    auto check_messages = [&] { return script.console_messages.size() > 0; };
    script.console_condition->wait(console_lock, stopper, check_messages);
    std::list<std::u16string> latest_messages{};
    script.console_messages.swap(latest_messages);
    for (const std::string &message :
         latest_messages | std::views::transform(convert_message))
      std::println("{}", message);
    if (stopper.stop_requested())
      return;
  };
  std::jthread console_thread{console_printer};

  return 0;
}
