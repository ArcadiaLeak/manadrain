const atom_literal_arr = [
  "const",
  "let",
  "var",
  "class",
  "function"
];

let offset = 0;
const encoder = new TextEncoder();
const atom_prealloc_pos = [];
const atom_prealloc_buf = [];

function LE_encode(num) {
  const buffer = new ArrayBuffer(4);
  const view = new DataView(buffer);
  view.setUint32(0, num, true);
  return new Uint8Array(buffer);
}

for (const lit of atom_literal_arr) {
  const lit_length = [
    ...LE_encode(lit.length),
    0, 0, 0, 0
  ];

  const lit_bytes = Array.from(encoder.encode(lit));
  lit_bytes.length = Math.ceil(lit.length / 8) * 8;
  lit_bytes.fill(0, lit.length);

  atom_prealloc_buf.push(...lit_length);
  atom_prealloc_buf.push(...lit_bytes);

  atom_prealloc_pos.push({
    offset,
    atom_name: lit
  });

  offset += lit_length.length;
  offset += lit_bytes.length;
}

Deno.writeTextFile("include/atom_prealloc.hpp", `\
#include <array>

namespace Interpret {
${atom_prealloc_pos
    .map(({ offset, atom_name }) =>
      `inline constexpr std::size_t S_ATOM_${atom_name}{${offset}};`)
    .join('\n')}

inline constexpr std::array<std::size_t, ${atom_prealloc_pos.length}> atom_prealloc_pos{{
  ${atom_prealloc_pos.map(({ atom_name }) => `S_ATOM_${atom_name}`).join(', ')}
}};

inline constexpr std::array<char, ${atom_prealloc_buf.length}> atom_prealloc_buf{{
  ${atom_prealloc_buf.map((ch_code) => ch_code.toString()).join(', ')}
}};
}
`);
