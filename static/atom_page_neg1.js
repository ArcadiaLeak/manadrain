const atom_literal_arr = [
  "const",
  "let",
  "var"
];

let offset = 0;
const encoder = new TextEncoder();
const neg1_page_pos = [];
const neg1_page_buf = [];

for (const lit of atom_literal_arr) {
  const ch_arr = Array.from(encoder.encode(lit));
  const aligned_length = Math.ceil(lit.length / 8);

  ch_arr.length = aligned_length * 8;
  ch_arr.fill(0, lit.length);
  neg1_page_buf.push(...ch_arr);

  neg1_page_pos.push({
    offset,
    length: lit.length,
    atom_name: lit
  });

  offset += aligned_length;
}

Deno.writeTextFile("include/atom_page_neg1.hpp", `\
#include <array>
#include <cstdint>

namespace Manadrain {
struct P_ATOM {
  std::int16_t pageid;
  std::uint16_t offset;
  std::uint16_t length;
  bool operator==(const P_ATOM&) const = default;
};

${neg1_page_pos
    .map(({ offset, length, atom_name }) =>
      `constexpr P_ATOM S_ATOM_${atom_name}{-1, ${offset}, ${length}};`)
    .join('\n')}

static const std::array<char, ${neg1_page_buf.length}> neg1_page_buf{{
  ${neg1_page_buf
    .map((ch_code) => ch_code.toString())
    .join(', ')}
}};
}
`);
