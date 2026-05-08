#include <cassert>
#include <ranges>
#include <span>

#include "machine.hpp"

namespace Manadrain {
void Machine::operator()(I32_ADD cmd) {
  std::int32_t lhs{register_file.back().sint};
  register_file.pop_back();
  std::int32_t rhs{register_file.back().sint};
  register_file.back().sint = lhs + rhs;
}

void Machine::operator()(I64_ADD cmd) {
  std::int64_t lhs{register_file.back().slong};
  register_file.pop_back();
  std::int64_t rhs{register_file.back().slong};
  register_file.back().slong = lhs + rhs;
}

void Machine::operator()(F32_ADD cmd) {
  std::float32_t lhs{register_file.back().float32};
  register_file.pop_back();
  std::float32_t rhs{register_file.back().float32};
  register_file.back().float32 = lhs + rhs;
}

void Machine::operator()(U64_LOC_LOAD cmd) {
  register_file.push_back(UNIFORM{.ulong = local_heap[cmd.offset >> 3].ulong});
}

void Machine::operator()(U64_LOC_STOR cmd) {
  local_heap[cmd.offset >> 3].ulong = register_file.back().ulong;
  register_file.pop_back();
}

void Machine::operator()(I32_PUSH cmd) {
  register_file.push_back(UNIFORM{.sint = cmd.val});
}

void Machine::operator()(I64_PUSH cmd) {
  register_file.push_back(UNIFORM{.slong = cmd.val});
}

void Machine::operator()(U64_PUSH cmd) {
  register_file.push_back(UNIFORM{.ulong = cmd.val});
}

void Machine::operator()(U64_TO_I32 cmd) {
  register_file.back().sint =
      static_cast<std::int32_t>(register_file.back().ulong);
}

void Machine::operator()(I64_TO_I32 cmd) {
  register_file.back().sint =
      static_cast<std::int32_t>(register_file.back().slong);
}

void Machine::operator()(std::size_t func_idx) {
  for (MACHINE_CMD cmd : function_vec[func_idx].command_vec) {
    cmd.visit(*this);
  }
}

std::size_t Machine::heap_alloc() {
  if (last_vacancy) {
    std::size_t idx{*last_vacancy};
    last_vacancy = std::get<HEAP_VACANCY>(global_heap[idx]).another;
    return idx;
  }
  auto heap_iter =
      global_heap.insert(global_heap.end(), std::vector<UNIFORM>{});
  return std::distance(global_heap.begin(), heap_iter);
}

void Machine::heap_free(std::size_t idx) {
  assert(idx < global_heap.size());
  assert(global_heap[idx].index() == 0);

  global_heap[idx] = HEAP_TOMBSTONE{};
  ++n_tombstones;

  if (n_tombstones >= (global_heap.size() >> 1))
    heap_reclaim();
}

bool Machine::is_tombstone_ptr(UNIFORM word) {
  std::size_t idx{word.ulong};
  return idx < global_heap.size() && global_heap[idx].index() == 1;
};

void Machine::heap_reclaim() {
  std::vector<bool> referenced{};
  referenced.resize(global_heap.size());

  auto united_heap = std::ranges::concat_view{
      local_heap, global_heap | std::views::transform([](HEAP_SLOT &slot) {
                    return slot.index() == 0 ? std::span{std::get<0>(slot)}
                                             : std::span<UNIFORM>{};
                  }) | std::views::join};
  for (UNIFORM w : united_heap)
    if (is_tombstone_ptr(w))
      referenced[w.ulong] = 1;

  for (std::size_t idx = 0; idx < global_heap.size(); ++idx)
    if (not referenced[idx] && global_heap[idx].index() == 1) {
      --n_tombstones;
      global_heap[idx] = HEAP_VACANCY{last_vacancy};
      last_vacancy = idx;
    }
}
} // namespace Manadrain
