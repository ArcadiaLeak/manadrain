#include "machine.hpp"

namespace Manadrain {
void Machine::operator()(I32_ADD cmd) {
  std::int32_t lhs{register_file[cmd.lhs >> 3].sint[(cmd.lhs >> 2) & 1]};
  std::int32_t rhs{register_file[cmd.rhs >> 3].sint[(cmd.rhs >> 2) & 1]};
  register_file[cmd.dest >> 3].sint[(cmd.dest >> 2) & 1] = lhs + rhs;
}

void Machine::operator()(I64_ADD cmd) {
  std::int64_t lhs{register_file[cmd.lhs >> 3].slong};
  std::int64_t rhs{register_file[cmd.rhs >> 3].slong};
  register_file[cmd.dest >> 3].slong = lhs + rhs;
}

void Machine::operator()(F32_ADD cmd) {
  std::float32_t lhs{register_file[cmd.lhs >> 3].float32[(cmd.lhs >> 2) & 1]};
  std::float32_t rhs{register_file[cmd.rhs >> 3].float32[(cmd.rhs >> 2) & 1]};
  register_file[cmd.dest >> 3].float32[(cmd.dest >> 2) & 1] = lhs + rhs;
}

void Machine::operator()(U64_LOC_LOAD cmd) {
  register_file[cmd.reg >> 3].ulong = local_heap[cmd.offset >> 3].ulong;
}

void Machine::operator()(U64_LOC_STOR cmd) {
  local_heap[cmd.offset >> 3].ulong = register_file[cmd.reg >> 3].ulong;
}

void Machine::operator()(std::size_t func_idx) {
  for (COMMAND cmd : function_vec[func_idx]) {
    cmd.visit(*this);
  }
}
} // namespace Manadrain
