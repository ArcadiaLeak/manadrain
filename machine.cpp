#include "machine.hpp"

namespace Manadrain {
void Machine::operator()(I32_ADD cmd) {
  std::int32_t lhs{std::bit_cast<std::int32_t>(
      static_cast<std::uint32_t>(register_file[cmd.lhs]))};
  std::int32_t rhs{std::bit_cast<std::int32_t>(
      static_cast<std::uint32_t>(register_file[cmd.rhs]))};
  std::int32_t ret{lhs + rhs};
  register_file[cmd.dest] = std::bit_cast<std::uint32_t>(ret);
}

void Machine::operator()(I64_ADD cmd) {
  std::int64_t lhs{std::bit_cast<std::int64_t>(register_file[cmd.lhs])};
  std::int64_t rhs{std::bit_cast<std::int64_t>(register_file[cmd.rhs])};
  std::int64_t ret{lhs + rhs};
  register_file[cmd.dest] = std::bit_cast<std::uint64_t>(ret);
}

void Machine::operator()(F32_ADD cmd) {
  std::float32_t lhs{std::bit_cast<std::float32_t>(
      static_cast<std::uint32_t>(register_file[cmd.lhs]))};
  std::float32_t rhs{std::bit_cast<std::float32_t>(
      static_cast<std::uint32_t>(register_file[cmd.rhs]))};
  std::float32_t ret{lhs + rhs};
  register_file[cmd.dest] = std::bit_cast<std::uint32_t>(ret);
}

void Machine::operator()(LOCAL_LOAD cmd) {
  register_file[cmd.reg] = local_heap[cmd.offset];
}

void Machine::operator()(LOCAL_STOR cmd) {
  local_heap[cmd.offset] = register_file[cmd.reg];
}

void Machine::operator()(std::size_t func_idx) {
  for (COMMAND cmd : function_vec[func_idx]) {
    cmd.visit(*this);
  }
}
} // namespace Manadrain
