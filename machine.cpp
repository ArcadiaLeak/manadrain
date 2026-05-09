#include <cassert>
#include <ranges>
#include <span>

#include "machine.hpp"

namespace Manadrain {
void Machine::operator()(I32_ADD cmd) {
  std::int32_t rhs{std::bit_cast<std::int32_t>(
      static_cast<std::uint32_t>(register_file.back()))};
  register_file.pop_back();
  std::int32_t lhs{std::bit_cast<std::int32_t>(
      static_cast<std::uint32_t>(register_file.back()))};
  register_file.back() = std::bit_cast<std::uint32_t>(lhs + rhs);
}

void Machine::operator()(I64_ADD cmd) {
  std::int64_t rhs{std::bit_cast<std::int64_t>(register_file.back())};
  register_file.pop_back();
  std::int64_t lhs{std::bit_cast<std::int64_t>(register_file.back())};
  register_file.back() = std::bit_cast<std::uint64_t>(lhs + rhs);
}

void Machine::operator()(F32_ADD cmd) {
  std::float32_t rhs{std::bit_cast<std::float32_t>(
      static_cast<std::uint32_t>(register_file.back()))};
  register_file.pop_back();
  std::float32_t lhs{std::bit_cast<std::float32_t>(
      static_cast<std::uint32_t>(register_file.back()))};
  register_file.back() = std::bit_cast<std::uint32_t>(lhs + rhs);
}

void Machine::operator()(I64_SUB cmd) {
  std::int64_t rhs{std::bit_cast<std::int64_t>(register_file.back())};
  register_file.pop_back();
  std::int64_t lhs{std::bit_cast<std::int64_t>(register_file.back())};
  register_file.back() = std::bit_cast<std::uint64_t>(lhs - rhs);
}

void Machine::operator()(LOC_LOAD cmd) {
  register_file.push_back(local_heap[cmd.offset]);
}

void Machine::operator()(LOC_STORE cmd) {
  local_heap[cmd.offset] = register_file.back();
  register_file.pop_back();
}

void Machine::operator()(LOC_APPEND cmd) {
  local_heap.push_back(register_file.back());
  register_file.pop_back();
}

void Machine::operator()(I32_PUSH cmd) {
  register_file.push_back(std::bit_cast<std::uint32_t>(cmd.val));
}

void Machine::operator()(I64_PUSH cmd) {
  register_file.push_back(std::bit_cast<std::uint64_t>(cmd.val));
}

void Machine::operator()(U64_PUSH cmd) { register_file.push_back(cmd.val); }

void Machine::operator()(U64_TO_I32 cmd) {
  register_file.back() = std::bit_cast<std::uint32_t>(
      static_cast<std::int32_t>(register_file.back()));
}

void Machine::operator()(I64_TO_I32 cmd) {
  register_file.back() = std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(
      std::bit_cast<std::int64_t>(register_file.back())));
}

void Machine::operator()(I32_TO_I64 cmd) {
  auto src_it = std::next(register_file.rbegin(), cmd.adv);
  std::int32_t src{
      std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(*src_it))};
  *src_it = static_cast<std::int64_t>(src);
}

void Machine::operator()(std::size_t func_idx) {
  for (MACHINE_CMD cmd : function_vec[func_idx].command_vec) {
    cmd.visit(*this);
  }
}

std::size_t Machine::heap_alloc() {
  if (last_vacancy) {
    std::size_t idx{*last_vacancy};
    last_vacancy = std::get<HEAP_VACANCY>(global_heap[idx].error()).another;
    return idx;
  }
  auto heap_iter = global_heap.emplace(global_heap.end());
  return std::distance(global_heap.begin(), heap_iter);
}

void Machine::heap_free(std::size_t idx) {
  assert(idx < global_heap.size());
  assert(global_heap[idx].has_value());

  global_heap[idx] = std::unexpected{HEAP_TOMBSTONE{}};
  ++n_tombstones;

  if (n_tombstones >= (global_heap.size() >> 1))
    heap_reclaim();
}

bool Machine::is_tombstone_ptr(std::uint64_t word) {
  return word < global_heap.size() && !global_heap[word] &&
         std::holds_alternative<HEAP_TOMBSTONE>(global_heap[word].error());
};

void Machine::heap_reclaim() {
  std::vector<bool> referenced{};
  referenced.resize(global_heap.size());

  auto united_heap = std::ranges::concat_view{
      local_heap, global_heap | std::views::transform([](HEAP_SLOT &slot) {
                    return slot.has_value() ? std::span{*slot}
                                            : std::span<std::uint64_t>{};
                  }) | std::views::join};
  for (std::uint64_t w : united_heap)
    if (is_tombstone_ptr(w))
      referenced[w] = 1;

  for (std::size_t idx = 0; idx < global_heap.size(); ++idx)
    if (not referenced[idx] && !global_heap[idx] &&
        std::holds_alternative<HEAP_TOMBSTONE>(global_heap[idx].error())) {
      --n_tombstones;
      global_heap[idx] = std::unexpected{HEAP_VACANCY{last_vacancy}};
      last_vacancy = idx;
    }
}
} // namespace Manadrain
