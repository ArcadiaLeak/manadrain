#include "machine.hpp"

Machine::Machine()
    : resource{std::make_unique<std::pmr::monotonic_buffer_resource>()} {}
