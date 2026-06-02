#include "machine.hpp"

int main(int argc, char *argv[]) {
  Machine machine{};

  std::pmr::polymorphic_allocator<> allocator(machine.resource.get());
  machine.function_frames.push_front(allocator.new_object<FunctionFrame>());
  for (FunctionFrame *function_frame : machine.function_frames)
    allocator.delete_object(function_frame);

  return 0;
}
