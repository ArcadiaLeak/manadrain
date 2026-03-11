#include <print>
#include <fstream>
#include <filesystem>
#include <string>
#include <cstdio>
#include <ranges>
#include <vector>
#include <unordered_map>
#include <deque>

#include "js_atom_enum.hpp"

struct JSAtom {};
struct JSString : JSAtom {};

struct JSRuntime {
  std::unordered_map<std::string, std::shared_ptr<JSString>> str_hash;
  std::deque<JSAtom> atom_deq;
};

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::println(stderr, "Usage: {} <filepath>", argv[0]);
    return 1;
  }

  std::string filePath = argv[1];

  if (!std::filesystem::exists(filePath)) {
    std::println(stderr, "Error: file does not exist: {}", filePath);
    return 1;
  }

  std::ifstream file(filePath);
  if (!file.is_open()) {
    std::println(stderr, "Error: could not open file: {}", filePath);
    return 1;
  }

  std::vector<char> vec = std::ranges::to<std::vector>(
    std::views::istream<char>(file >> std::noskipws)
  );

  std::shared_ptr<char[]> shared_arr = std::make_shared<char[]>(vec.size());
  std::copy(vec.begin(), vec.end(), shared_arr.get());

  std::print("{}", std::string_view{shared_arr.get(), vec.size()});

  std::ranges::repeat_view rep{'\0'};
  std::ranges::concat_view con{vec, rep};
  std::string infstr = std::ranges::to<std::string>(
    con | std::views::take(vec.size() + 5)
  );

  auto it = infstr.begin();

  std::print("{}", infstr);

  return 0;
}
