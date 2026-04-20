const atom_literal_arr = [
  "const",
  "let",
  "var"
];

let offset = 0;
const encoder = new TextEncoder();
const neg1_page_pos = [];
const neg1_page_buf = [];

function LE_encode(num) {
  const buffer = new ArrayBuffer(4);
  const view = new DataView(buffer);
  view.setUint32(0, num, true);
  return new Uint8Array(buffer);
}

for (const lit of atom_literal_arr) {
  const lit_length = [
    0, 0, 0, 0,
    ...LE_encode(lit.length)
  ];

  const lit_bytes = Array.from(encoder.encode(lit));
  lit_bytes.length = Math.ceil(lit.length / 8) * 8;
  lit_bytes.fill(0, lit.length);

  neg1_page_buf.push(...lit_length);
  neg1_page_buf.push(...lit_bytes);

  neg1_page_pos.push({
    offset,
    atom_name: lit
  });

  offset += lit_length.length;
  offset += lit_bytes.length;
}

Deno.writeTextFile("include/atom_page_neg1.hpp", `\
#include <array>

namespace Manadrain {
${neg1_page_pos
    .map(({ offset, atom_name }) =>
      `constexpr std::size_t S_ATOM_${atom_name}{${offset}};`)
    .join('\n')}

static const std::array<char, ${neg1_page_buf.length}> neg1_page_buf{{
  ${neg1_page_buf
    .map((ch_code) => ch_code.toString())
    .join(', ')}
}};
}
`);
