const atom_literal_arr = [
  "const",
  "let",
  "var"
];

let offset = 0;
const encoder = new TextEncoder();
const atom_zero_pos = [];
const atom_zero_buf = [];

for (const lit of atom_literal_arr) {
  const ch_arr = Array.from(encoder.encode(lit));
  const aligned_length = Math.ceil(lit.length / 8);

  ch_arr.length = aligned_length * 8;
  ch_arr.fill(0, lit.length);
  atom_zero_buf.push(...ch_arr);

  atom_zero_pos.push({
    offset,
    length: lit.length
  });

  offset += aligned_length;
}

const decl_zero_pos = `\
const std::array<std::pair<std::uint16_t, std::uint16_t>, ${atom_zero_pos.length}>
    atom_zero_pos\
`;
const decl_zero_buf = `\
const std::array<char, ${atom_zero_buf.length}> atom_zero_buf\
`;

const hpp_code = `\
#include <array>
#include <cstdint>

namespace Manadrain {
extern ${decl_zero_pos};
extern ${decl_zero_buf};
}
`;

const cpp_code = `\
#include "atom_zero_page.hpp"

namespace Manadrain {
${decl_zero_pos}{{
  ${atom_zero_pos
    .map(({ offset, length }) => `{ ${offset}, ${length} }`)
    .join(', ')}
}};

${decl_zero_buf}{{
  ${atom_zero_buf
    .map((ch_code) => ch_code.toString())
    .join(', ')}
}};
}
`;

Deno.writeTextFile("include/atom_zero_page.hpp", hpp_code);
Deno.writeTextFile("src/atom_zero_page.cpp", cpp_code);
