#include "machine.hpp"

namespace Manadrain {
namespace Machine {
void Execution::operator()(I32_ADD cmd) {
  std::int32_t lhs{std::bit_cast<std::int32_t>(
      static_cast<std::uint32_t>(regfile.at(cmd.lhs)))};
  std::int32_t rhs{std::bit_cast<std::int32_t>(
      static_cast<std::uint32_t>(regfile.at(cmd.rhs)))};
  std::int32_t ret{lhs + rhs};
  regfile.at(cmd.dest) = std::bit_cast<std::uint32_t>(ret);
}

void Execution::operator()(I64_ADD cmd) {
  std::int64_t lhs{std::bit_cast<std::int64_t>(regfile.at(cmd.lhs))};
  std::int64_t rhs{std::bit_cast<std::int64_t>(regfile.at(cmd.rhs))};
  std::int64_t ret{lhs + rhs};
  regfile.at(cmd.dest) = std::bit_cast<std::uint64_t>(ret);
}

void Execution::operator()(F32_ADD cmd) {
  std::float32_t lhs{std::bit_cast<std::float32_t>(
      static_cast<std::uint32_t>(regfile.at(cmd.lhs)))};
  std::float32_t rhs{std::bit_cast<std::float32_t>(
      static_cast<std::uint32_t>(regfile.at(cmd.rhs)))};
  std::float32_t ret{lhs + rhs};
  regfile.at(cmd.dest) = std::bit_cast<std::uint32_t>(ret);
}

void Execution::operator()(LOCAL_LOAD cmd) {
  regfile.at(cmd.reg) = local_heap.at(cmd.offset);
}

void Execution::operator()(LOCAL_STOR cmd) {
  local_heap.at(cmd.offset) = regfile.at(cmd.reg);
}

void Execution::operator()() {
  for (COMMAND cmd : script) {
    cmd.visit(*this);
  }
}
} // namespace Machine
} // namespace Manadrain
