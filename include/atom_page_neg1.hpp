#include <array>
#include <cstdint>

namespace Manadrain {
struct P_ATOM {
  std::int16_t pageid;
  std::uint16_t offset;
  std::uint16_t length;
  bool operator==(const P_ATOM &) const = default;
};

constexpr P_ATOM S_ATOM_const{-1, 0, 5};
constexpr P_ATOM S_ATOM_let{-1, 1, 3};
constexpr P_ATOM S_ATOM_var{-1, 2, 3};

static const std::array<char, 24> neg1_page_buf{
    {99, 111, 110, 115, 116, 0,  0,   0, 108, 101, 116, 0,
     0,  0,   0,   0,   118, 97, 114, 0, 0,   0,   0,   0}};
} // namespace Manadrain
