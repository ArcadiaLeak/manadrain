#include <array>
#include <cstdint>

namespace Manadrain {
struct S_ATOM {
  std::uint16_t offset;
  std::uint16_t length;
};

constexpr S_ATOM S_ATOM_CONST{0, 5};
constexpr S_ATOM S_ATOM_LET{1, 3};
constexpr S_ATOM S_ATOM_VAR{2, 3};

static const std::array<char, 24> atom_zero_buf{
    {99, 111, 110, 115, 116, 0,  0,   0, 108, 101, 116, 0,
     0,  0,   0,   0,   118, 97, 114, 0, 0,   0,   0,   0}};
} // namespace Manadrain
