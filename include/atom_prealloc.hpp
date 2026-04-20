#include <array>

namespace Manadrain {
constexpr std::size_t S_ATOM_const{0};
constexpr std::size_t S_ATOM_let{16};
constexpr std::size_t S_ATOM_var{32};

constexpr std::array<char, 48> atom_prealloc_buf{
    {0, 0, 0, 0, 5, 0, 0, 0, 99,  111, 110, 115, 116, 0, 0, 0,
     0, 0, 0, 0, 3, 0, 0, 0, 108, 101, 116, 0,   0,   0, 0, 0,
     0, 0, 0, 0, 3, 0, 0, 0, 118, 97,  114, 0,   0,   0, 0, 0}};
} // namespace Manadrain
