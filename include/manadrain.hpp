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
enum BAD_TOKEN { UNEXPECTED_COMMENT_END };

struct TOK_LET {};
struct TOK_CONST {};
struct TOK_VAR {};
struct TOK_EOF {};
using TOKEN_TYPE = std::variant<TOK_LET, TOK_CONST, TOK_VAR, TOK_EOF>;

struct TOKEN_STRING {
  char32_t sep;
  std::string content;
};

struct TOKEN_IDENT {
  bool has_escape;
  std::string content;
};

struct TOKEN {
  bool newline_seen;
  TOKEN_TYPE type;
  TOKEN_STRING data_string;
  TOKEN_IDENT data_ident;
};

struct ParseState {
  std::uint32_t idx;
};

struct ParseDriver {
  const std::basic_string<std::uint8_t> buffer;
  ParseState state;

  std::optional<char32_t> peek();
  std::optional<char32_t> shift();
  void drop(std::uint32_t count);

  bool parseHex_dang(std::uint32_t& digit);
  bool parseHex(std::uint32_t& digit);

  bool parseEscape_dang(ESC_RULE esc_rule,
                        std::pair<char32_t, BAD_ESCAPE>& either);
  bool parseEscape(ESC_RULE esc_rule, std::pair<char32_t, BAD_ESCAPE>& either);
  bool parseEscape_hex(std::pair<char32_t, BAD_ESCAPE>& either);
  bool parseEscape_uni(ESC_RULE esc_rule,
                       std::pair<char32_t, BAD_ESCAPE>& either);
  bool parseEscape_braceSeq(std::pair<char32_t, BAD_ESCAPE>& either);
  bool parseEscape_fixedSeq(ESC_RULE esc_rule,
                            std::pair<char32_t, BAD_ESCAPE>& either);

  bool parseString(STRICTNESS strictness, TOKEN_STRING& token, BAD_STRING& err);
  int parseString_escSeq_dang(STRICTNESS strictness,
                              char32_t sep,
                              std::pair<char32_t, BAD_STRING>& either);
  int parseString_escSeq(STRICTNESS strictness,
                         char32_t sep,
                         std::pair<char32_t, BAD_STRING>& either);

  bool parseIdent(TOKEN_IDENT& ident, bool is_private);
  bool parseIdent_uchar(TOKEN_IDENT& ident, bool beginning);

  bool parseToken_dang(TOKEN& token, std::variant<BAD_TOKEN, BAD_ESCAPE>& err);
};
}  // namespace Manadrain
