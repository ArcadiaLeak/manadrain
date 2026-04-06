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

struct TOKEN {
  struct TYPE_LET {};
  struct TYPE_CONST {};
  struct TYPE_VAR {};
  struct TYPE_EOF {};
  struct TYPE_IDENT {};
  struct TYPE_STRING {};
  struct TYPE_ERROR {};
  using TYPE = std::variant<TYPE_LET,
                            TYPE_CONST,
                            TYPE_VAR,
                            TYPE_EOF,
                            TYPE_IDENT,
                            TYPE_STRING,
                            TYPE_ERROR>;

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

  bool newline_seen;
  TYPE type;
  PAYLOAD_STR str;
  PAYLOAD_IDENT ident;
  PAYLOAD_ERR err;
};

struct ATOM_STAT {
  struct RESERVED {
    TOKEN::TYPE token_type;
    STRICTNESS strictness;
  };
  struct INTRINSIC {};
  using CATEGORY = std::variant<RESERVED, INTRINSIC>;

  std::string_view lit_view;
  CATEGORY category;
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

  bool parseString(TOKEN::PAYLOAD_STR& token, BAD_STRING& err);
  int parseString_escSeq_dang(char32_t sep,
                              std::pair<char32_t, BAD_STRING>& either);
  int parseString_escSeq(char32_t sep, std::pair<char32_t, BAD_STRING>& either);

  bool parseIdent(TOKEN::PAYLOAD_IDENT& ident, bool is_private);
  bool parseIdent_uchar(TOKEN::PAYLOAD_IDENT& ident, bool beginning);

  bool parseToken_dang(TOKEN& token);
  bool parseToken(TOKEN& token);
  bool tryCv_reserved(TOKEN& token);
  bool tryCv_intrinsic(TOKEN& token);
};
}  // namespace Manadrain
