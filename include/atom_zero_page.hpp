#include <array>
#include <cstdint>

namespace Manadrain {
struct P_ATOM {
  std::uint16_t pageid;
  std::uint16_t offset;
  std::uint16_t length;
  bool operator==(const P_ATOM &) const = default;
};

constexpr P_ATOM S_ATOM_const{0, 0, 5};
constexpr P_ATOM S_ATOM_let{0, 1, 3};
constexpr P_ATOM S_ATOM_var{0, 2, 3};

static const std::array<char, 24> atom_zero_buf{
    {99, 111, 110, 115, 116, 0,  0,   0, 108, 101, 116, 0,
     0,  0,   0,   0,   118, 97, 114, 0, 0,   0,   0,   0}};
} // namespace Manadrain
