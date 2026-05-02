const atom_literal_arr = [
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
${atom_literal_arr
    .map((atom_s, atom_idx) =>
      `inline constexpr std::size_t S_ATOM_${atom_s}{${atom_idx}};`)
    .join('\n')}

static const std::array<std::string_view, ${atom_literal_arr.length}> atom_prealloc{{
  ${atom_literal_arr.map((atom_s) => `"${atom_s}"`).join(', ')}
}};
}
}
`);
