#include "machine.hpp"

static const std::array permanent_chars{
    std::to_array<char>({0x68, 0x65, 0x6c, 0x6c, 0x6f})};
static const std::array permanent_views{
    std::to_array<std::string_view>({{permanent_chars.data() + 0, 5}})};

int main(int argc, char *argv[]) {
  Machine machine{};

  std::pmr::polymorphic_allocator<> allocator(&machine.resource);
  machine.function_frames.push_front(allocator.new_object<FunctionFrame>());
  for (FunctionFrame *function_frame : machine.function_frames)
    allocator.delete_object(function_frame);

  return 0;
}
