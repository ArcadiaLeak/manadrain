#include <iostream>
#include <print>

#include "machine.hpp"

int main(int argc, char *argv[]) {
  Manadrain::Machine::Execution execution{};
  execution.register_file.resize(256);
  execution.register_file[0] = std::bit_cast<std::uint32_t>(std::int32_t{-5});
  execution.register_file[1] = 7;
  execution.script.push_back(Manadrain::Machine::ADD_INT32{2, 0, 1});
  execution();

  std::println(std::cout, "Hello, world!");
  return 0;
}
