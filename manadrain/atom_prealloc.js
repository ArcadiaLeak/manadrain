const reserved_word_arr = [
  "const",
  "let",
  "var",
  "class",
  "function",
  "return",
  "import",
  "export",
  "from",
  "as",
  "default",
  "undefined",
  "null",
  "true",
  "false"
];

Deno.writeTextFile("manadrain/atom_prealloc.hpp", `\
#include <array>

namespace Manadrain {
namespace Syntax {
${reserved_word_arr
    .map((atom_s, atom_idx) =>
      `inline constexpr std::size_t S_ATOM_${atom_s}{${atom_idx}};`)
    .join('\n')}

static const std::array<std::string_view, ${reserved_word_arr.length}> atom_prealloc{{
  ${reserved_word_arr.map((atom_s) => `"${atom_s}"`).join(', ')}
}};

static bool is_reserved(std::size_t atom_idx) {
  switch (atom_idx) {
    ${reserved_word_arr
    .map((atom_s) => `case S_ATOM_${atom_s}:`)
    .join('\n')}
      return 1;
    default:
      return 0;
  }
}
}
}
`);
