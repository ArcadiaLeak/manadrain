const reserved_words = [
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
  "false",
  "if",
  "else",
  "while",
  "for",
  "do",
  "break",
  "continue",
  "switch",
  "int",
  "long",
  "uint",
  "ulong",
  "float",
  "double",
];

Deno.writeTextFile(
  "include/static_atoms.hpp",
  `\
#include <array>

namespace Manadrain {
${reserved_words
  .map(
    (atom_s, atom_idx) =>
      `inline constexpr std::size_t S_ATOM_${atom_s}{${atom_idx}};`,
  )
  .join("\n")}

static const std::array<std::string_view, ${reserved_words.length}> S_ATOM_ARR{{
  ${reserved_words.map((atom_s) => `"${atom_s}"`).join(", ")}
}};

static bool is_reserved(std::size_t atom_idx) {
  switch (atom_idx) {
    ${reserved_words.map((atom_s) => `case S_ATOM_${atom_s}:`).join("\n")}
      return 1;
    default:
      return 0;
  }
}
}
`,
);
