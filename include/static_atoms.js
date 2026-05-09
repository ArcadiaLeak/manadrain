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
  "string",
];
const anchor_words = ["size"];
const known_words = reserved_words.concat(anchor_words);

Deno.writeTextFile(
  "include/static_atoms.hpp",
  `\
#include <array>

namespace Manadrain {
${known_words
  .map(
    (atom_s, atom_idx) =>
      `inline constexpr std::size_t S_ATOM_${atom_s}{${atom_idx}};`,
  )
  .join("\n")}

static const std::array<std::string_view, ${known_words.length}> S_ATOM_ARR{{
  ${known_words.map((atom_s) => `"${atom_s}"`).join(", ")}
}};

static bool is_reserved(std::size_t atom_idx) {
  return atom_idx < ${reserved_words.length};
}
}
`,
);
