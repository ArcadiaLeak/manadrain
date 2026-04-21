#include <array>

namespace Manadrain {
inline constexpr std::size_t S_ATOM_const{0};
inline constexpr std::size_t S_ATOM_let{16};
inline constexpr std::size_t S_ATOM_var{32};

inline constexpr std::array<std::size_t, 3> atom_prealloc_pos{
    {S_ATOM_const, S_ATOM_let, S_ATOM_var}};

inline constexpr std::array<char, 48> atom_prealloc_buf{
    {5, 0, 0, 0, 0, 0, 0, 0, 99,  111, 110, 115, 116, 0, 0, 0,
     3, 0, 0, 0, 0, 0, 0, 0, 108, 101, 116, 0,   0,   0, 0, 0,
     3, 0, 0, 0, 0, 0, 0, 0, 118, 97,  114, 0,   0,   0, 0, 0}};
} // namespace Manadrain
