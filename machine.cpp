#include <cassert>

#include "machine.hpp"

namespace Manadrain {
Machine::SHARED_HEAP::CONTAINER::node_type &
Machine::heap_get(std::uint64_t handle) {
  for (SHARED_HEAP::CONTAINER::node_type &heap_node : shared_heap.cache) {
    if (heap_node.empty() || handle != heap_node.key())
      continue;
    return heap_node;
  }
  SHARED_HEAP::CONTAINER::node_type heap_node{
      shared_heap.entry_umap.extract(handle)};
  if (heap_node.empty())
    throw NULL_HANDLE_ERROR{};
  if (not shared_heap.cache[shared_heap.last_extracted].empty())
    shared_heap.last_extracted = not shared_heap.last_extracted;
  heap_node.swap(shared_heap.cache[shared_heap.last_extracted]);
  shared_heap.entry_umap.insert(std::move(heap_node));
  return shared_heap.cache[shared_heap.last_extracted];
}

std::size_t Machine::heap_alloc() {
  std::size_t handle{shared_heap.counter++};
  shared_heap.entry_umap.try_emplace(handle);
  return handle;
}

void Machine::heap_free(std::size_t handle) {
  SHARED_HEAP::CONTAINER::node_type empty_node{};
  empty_node.swap(heap_get(handle));
}
} // namespace Manadrain
