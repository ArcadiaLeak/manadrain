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
    length: lit.length,
    atom_name: lit.toUpperCase()
  });

  offset += aligned_length;
}

Deno.writeTextFile("include/atom_zero_page.hpp", `\
#include <array>
#include <cstdint>

namespace Manadrain {
struct S_ATOM {
  std::uint16_t offset;
  std::uint16_t length;
};

${atom_zero_pos
    .map(({ offset, length, atom_name }) =>
      `constexpr S_ATOM S_ATOM_${atom_name}{${offset}, ${length}};`)
    .join('\n')}

static const std::array<char, ${atom_zero_buf.length}> atom_zero_buf{{
  ${atom_zero_buf
    .map((ch_code) => ch_code.toString())
    .join(', ')}
}};
}
`);
