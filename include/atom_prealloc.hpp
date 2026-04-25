#include <array>

namespace Interpret {
inline constexpr std::size_t S_ATOM_const{0};
inline constexpr std::size_t S_ATOM_let{16};
inline constexpr std::size_t S_ATOM_var{32};
inline constexpr std::size_t S_ATOM_class{48};
inline constexpr std::size_t S_ATOM_function{64};

inline constexpr std::array<std::size_t, 5> atom_prealloc_pos{
    {S_ATOM_const, S_ATOM_let, S_ATOM_var, S_ATOM_class, S_ATOM_function}};

inline constexpr std::array<char, 80> atom_prealloc_buf{
    {5, 0, 0, 0, 0, 0, 0, 0, 99,  111, 110, 115, 116, 0,   0,   0,
     3, 0, 0, 0, 0, 0, 0, 0, 108, 101, 116, 0,   0,   0,   0,   0,
     3, 0, 0, 0, 0, 0, 0, 0, 118, 97,  114, 0,   0,   0,   0,   0,
     5, 0, 0, 0, 0, 0, 0, 0, 99,  108, 97,  115, 115, 0,   0,   0,
     8, 0, 0, 0, 0, 0, 0, 0, 102, 117, 110, 99,  116, 105, 111, 110}};
} // namespace Interpret
