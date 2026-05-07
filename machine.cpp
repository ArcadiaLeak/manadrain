#include <cassert>
#include <ranges>
#include <span>

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
