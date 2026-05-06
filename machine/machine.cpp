#include "machine.hpp"

namespace Manadrain {
namespace Machine {
void Execution::operator()(I32_ADD cmd) {
  std::int32_t lhs{std::bit_cast<std::int32_t>(
      static_cast<std::uint32_t>(register_file[cmd.lhs]))};
  std::int32_t rhs{std::bit_cast<std::int32_t>(
      static_cast<std::uint32_t>(register_file[cmd.rhs]))};
  std::int32_t ret{lhs + rhs};
  register_file[cmd.dest] = std::bit_cast<std::uint32_t>(ret);
}

void Execution::operator()(I64_ADD cmd) {
  std::int64_t lhs{std::bit_cast<std::int64_t>(register_file[cmd.lhs])};
  std::int64_t rhs{std::bit_cast<std::int64_t>(register_file[cmd.rhs])};
  std::int64_t ret{lhs + rhs};
  register_file[cmd.dest] = std::bit_cast<std::uint64_t>(ret);
}

void Execution::operator()(F32_ADD cmd) {
  std::float32_t lhs{std::bit_cast<std::float32_t>(
      static_cast<std::uint32_t>(register_file[cmd.lhs]))};
  std::float32_t rhs{std::bit_cast<std::float32_t>(
      static_cast<std::uint32_t>(register_file[cmd.rhs]))};
  std::float32_t ret{lhs + rhs};
  register_file[cmd.dest] = std::bit_cast<std::uint32_t>(ret);
}

void Execution::operator()(LOCAL_LOAD cmd) {
  register_file[cmd.reg] = local_heap[cmd.offset];
}

void Execution::operator()(LOCAL_STOR cmd) {
  local_heap[cmd.offset] = register_file[cmd.reg];
}

void Execution::operator()() {
  for (COMMAND cmd : script) {
    cmd.visit(*this);
  }
}
} // namespace Machine
} // namespace Manadrain
