#include <deque>
#include <string>
#include <unordered_map>
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
  MALFORMED_SEQ_IN_ESCAPE
};
enum class BAD_COMMENT { UNEXPECTED_END };

enum class TOKEN_TYPE {
  T_LET,
  T_CONST,
  T_VAR,
  T_EOF,
  T_IDENT,
  T_STRING,
  T_ERROR,
  T_UCHAR
};

struct Token {
  struct PAYLOAD_STR {
    char32_t sep;
    std::size_t pool_idx;
  };
  struct PAYLOAD_IDENT {
    bool has_escape;
    bool is_reserved;
    std::size_t pool_idx;
  };
  using PAYLOAD_ERR = std::variant<std::monostate, BAD_STRING, BAD_COMMENT>;

  TOKEN_TYPE type;
  bool newline_seen;
  char32_t uchar;
  PAYLOAD_STR str;
  PAYLOAD_IDENT ident;
  PAYLOAD_ERR err;

  bool is_pseudo_keyword(TOKEN_TYPE tok_type);
};

enum class VARDECL_KIND { K_LET, K_CONST, K_VAR };
struct STMT_VARDECL {
  VARDECL_KIND kind;
  Token::PAYLOAD_IDENT ident;
  Token::PAYLOAD_STR init;
};

struct ParseState {
  std::uint32_t idx;
  STRICTNESS strictness;
};

struct ParseDriver {
  const std::basic_string<std::uint8_t> buffer;
  ParseState state;

  std::unordered_map<std::string_view, std::size_t> atom_umap;
  std::deque<std::string> atom_deq;

  std::string ch_temp;
  std::size_t makeAtom_fromTemp();

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

  bool parseString(Token::PAYLOAD_STR& token, BAD_STRING& err);
  int parseString_escSeq_dang(char32_t sep,
                              std::pair<char32_t, BAD_STRING>& either);
  int parseString_escSeq(char32_t sep, std::pair<char32_t, BAD_STRING>& either);

  bool parseIdent(Token::PAYLOAD_IDENT& ident, bool is_private);
  bool parseIdent_uchar(Token::PAYLOAD_IDENT& ident, bool beginning);

  bool parseToken_dang(Token& token);
  bool parseToken(Token& token);

  bool tryReserved_ident(Token& token);
  bool tryReserved_string(Token& token);

  bool parseVardecl(STMT_VARDECL& vardecl);
  bool parseStatement();
};
}  // namespace Manadrain
