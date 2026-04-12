#include "atom_zero_page.hpp"

namespace Manadrain {
const std::array<std::pair<std::uint16_t, std::uint16_t>, 3> atom_zero_pos{
    {{0, 5}, {1, 3}, {2, 3}}};

const std::array<char, 24> atom_zero_buf{{99,  111, 110, 115, 116, 0, 0, 0,
                                          108, 101, 116, 0,   0,   0, 0, 0,
                                          118, 97,  114, 0,   0,   0, 0, 0}};
} // namespace Manadrain
