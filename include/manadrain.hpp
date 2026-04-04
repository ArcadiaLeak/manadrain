#include <string>
#include <variant>

namespace Manadrain {
enum class STRICTNESS { SLOPPY, STRICT };
enum class ESC_RULE {
  IDENTIFIER,
  REGEXP_ASCII,
  REGEXP_UTF16,
  STRING_IN_SLOPPY_MODE,
  STRING_IN_STRICT_MODE,
  STRING_IN_TEMPLATE
};

enum class BAD_ESCAPE { MALFORMED, PER_SE_BACKSLASH, OCTAL_SEQ };
enum class BAD_STRING {
  UNEXPECTED_END,
  OCTAL_SEQ_IN_ESCAPE,
  MALFORMED_SEQ_IN_ESCAPE,
  MISMATCH
};

struct TOKEN_STRING {
  char32_t sep;
  std::string content;
};

struct TOKEN_WORD {
  bool ident_has_escape;
  bool is_private;
  std::string content;
};

struct NODE_VARDECL {
  struct KIND_LET {};
  struct KIND_CONST {};
  struct KIND_VAR {};
  using KIND = std::variant<KIND_LET, KIND_CONST, KIND_VAR>;

  KIND kind;
  TOKEN_WORD name;
  TOKEN_STRING init;
};

struct ParseState {
  std::uint32_t idx;
  bool newline_seen;
};

struct ParseDriver {
  const std::basic_string<std::uint8_t> buffer;
  ParseState state;

  std::optional<char32_t> peek();
  std::optional<char32_t> shift();
  std::u32string_view take(std::uint32_t count, std::u32string& buf);
  void drop(std::uint32_t count);

  bool parseSpace();

  bool parseHex(std::uint32_t& digit);
  bool parseHex_b(std::uint32_t& digit);

  bool parseEscape(ESC_RULE esc_rule, std::pair<char32_t, BAD_ESCAPE>& either);
  bool parseEscape_b(ESC_RULE esc_rule,
                     std::pair<char32_t, BAD_ESCAPE>& either);
  bool parseEscape_hex(std::pair<char32_t, BAD_ESCAPE>& either);
  bool parseEscape_uni(ESC_RULE esc_rule,
                       std::pair<char32_t, BAD_ESCAPE>& either);
  bool parseEscape_braceSeq(std::pair<char32_t, BAD_ESCAPE>& either);
  bool parseEscape_fixedSeq(ESC_RULE esc_rule,
                            std::pair<char32_t, BAD_ESCAPE>& either);

  bool parseString(STRICTNESS strictness, TOKEN_STRING& token, BAD_STRING& err);
  int parseString_escape(STRICTNESS strictness,
                         char32_t sep,
                         std::pair<char32_t, BAD_STRING>& either);
  int parseString_escape_b(STRICTNESS strictness,
                           char32_t sep,
                           std::pair<char32_t, BAD_STRING>& either);

  bool parseWord(bool is_private, TOKEN_WORD& word);
  bool parseWord_idContinue(char32_t& ch, TOKEN_WORD& word);

  bool parseVardecl(NODE_VARDECL& vardecl);
  bool parseStatement();
};
}  // namespace Manadrain
