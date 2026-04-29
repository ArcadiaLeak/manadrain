#include <array>

namespace Interpret {
inline constexpr std::size_t S_ATOM_const{0};
inline constexpr std::size_t S_ATOM_let{16};
inline constexpr std::size_t S_ATOM_var{32};
inline constexpr std::size_t S_ATOM_class{48};
inline constexpr std::size_t S_ATOM_function{64};
inline constexpr std::size_t S_ATOM_return{80};
inline constexpr std::size_t S_ATOM_import{96};
inline constexpr std::size_t S_ATOM_export{112};
inline constexpr std::size_t S_ATOM_from{128};
inline constexpr std::size_t S_ATOM_as{144};
inline constexpr std::size_t S_ATOM_default{160};

inline constexpr std::array<std::size_t, 11> atom_prealloc_pos{
    {S_ATOM_const, S_ATOM_let, S_ATOM_var, S_ATOM_class, S_ATOM_function,
     S_ATOM_return, S_ATOM_import, S_ATOM_export, S_ATOM_from, S_ATOM_as,
     S_ATOM_default}};

inline constexpr std::array<char, 176> atom_prealloc_buf{
    {5, 0, 0, 0, 0, 0, 0, 0, 99,  111, 110, 115, 116, 0,   0,   0,
     3, 0, 0, 0, 0, 0, 0, 0, 108, 101, 116, 0,   0,   0,   0,   0,
     3, 0, 0, 0, 0, 0, 0, 0, 118, 97,  114, 0,   0,   0,   0,   0,
     5, 0, 0, 0, 0, 0, 0, 0, 99,  108, 97,  115, 115, 0,   0,   0,
     8, 0, 0, 0, 0, 0, 0, 0, 102, 117, 110, 99,  116, 105, 111, 110,
     6, 0, 0, 0, 0, 0, 0, 0, 114, 101, 116, 117, 114, 110, 0,   0,
     6, 0, 0, 0, 0, 0, 0, 0, 105, 109, 112, 111, 114, 116, 0,   0,
     6, 0, 0, 0, 0, 0, 0, 0, 101, 120, 112, 111, 114, 116, 0,   0,
     4, 0, 0, 0, 0, 0, 0, 0, 102, 114, 111, 109, 0,   0,   0,   0,
     2, 0, 0, 0, 0, 0, 0, 0, 97,  115, 0,   0,   0,   0,   0,   0,
     7, 0, 0, 0, 0, 0, 0, 0, 100, 101, 102, 97,  117, 108, 116, 0}};
} // namespace Interpret
