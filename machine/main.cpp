#include <iostream>
#include <print>

#include "machine.hpp"

int main(int argc, char *argv[]) {
  Manadrain::Machine::Execution execution{};
  execution.local_heap.resize(256);
  execution.local_heap[0] = std::bit_cast<std::uint32_t>(std::float32_t{-5.5f});
  execution.local_heap[1] = std::bit_cast<std::uint32_t>(std::float32_t{0.5f});
  execution.script.push_back(Manadrain::Machine::LOCAL_LOAD{0, 0});
  execution.script.push_back(Manadrain::Machine::LOCAL_LOAD{1, 1});
  execution.script.push_back(Manadrain::Machine::F32_ADD{2, 0, 1});
  execution.script.push_back(Manadrain::Machine::LOCAL_STOR{2, 2});
  execution();
  std::float32_t result{std::bit_cast<std::float32_t>(
      static_cast<std::uint32_t>(execution.local_heap[2]))};

  std::println(std::cout, "Hello, world!");
  return 0;
}
