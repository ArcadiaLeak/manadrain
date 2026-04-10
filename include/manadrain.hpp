#include <array>
#include <cstdint>
#include <deque>
#include <expected>
#include <string>
#include <unordered_map>
#include <variant>

namespace Manadrain {
enum class STRICTNESS { SLOPPY, STRICT };

enum TOKEN_KIND {
  K_TOKEN_EOF,
  K_TOKEN_LET,
  K_TOKEN_CONST,
  K_TOKEN_VAR,
  K_TOKEN_IDENT,
  K_TOKEN_STRING,
  K_TOKEN_UCHAR
};

struct TOKEN {
  struct PAYLOAD_STR {
    char32_t sep;
    std::size_t atom_idx;
  };
  struct PAYLOAD_IDENT {
    bool has_escape;
    bool is_reserved;
    std::size_t atom_idx;
  };

  TOKEN_KIND kind;
  bool is_pseudo_kind(int rhs_kind);

  bool newline_seen;
  char32_t uchar;
  PAYLOAD_STR str;
  PAYLOAD_IDENT ident;
};

struct EXPR_IDENT {
  std::size_t atom_idx;
};
using EXPRESSION = std::variant<EXPR_IDENT>;

struct STMT_VARDECL {
  int intro;
  TOKEN::PAYLOAD_IDENT ident;
  TOKEN::PAYLOAD_STR init;
};
using STATEMENT = std::variant<STMT_VARDECL, EXPRESSION>;

enum class ESC_RULE {
  IDENTIFIER,
  REGEXP_ASCII,
  REGEXP_UTF16,
  STRING_IN_SLOPPY_MODE,
  STRING_IN_STRICT_MODE,
  STRING_IN_TEMPLATE
};

struct MOVE_BUFIDX {
  enum ERRCODE { OUT_OF_RANGE };
};
struct PARSE_HEX {
  enum ERRCODE { NOT_A_DIGIT = MOVE_BUFIDX::OUT_OF_RANGE + 1 };
};
struct PARSE_ESCAPE {
  enum ERRCODE {
    MALFORMED = PARSE_HEX::NOT_A_DIGIT + 1,
    LEGACY_OCTAL_SEQ,
    NOT_AN_OCTAL_DIGIT,
    PER_SE_BACKSLASH
  };
  ESC_RULE rule;
};
struct PARSE_STRING {
  enum ERRCODE {
    UNEXPECTED_END = PARSE_ESCAPE::PER_SE_BACKSLASH + 1,
    LEGACY_OCTAL_SEQ,
    MALFORMED_ESC,
    MUST_CONTINUE
  };
};
struct PARSE_TOKEN {
  enum ERRCODE { UNCLOSED_COMMENT = PARSE_STRING::MUST_CONTINUE + 1 };
};
struct PARSE_IDENT {
  bool is_private;
};
struct PARSE_STMT {};

template <typename T>
using EXPECT = std::expected<T, int>;

struct ParseDriver {
  std::basic_string<std::uint8_t> buffer;
  std::int32_t buffer_idx;

  std::string ch_temp;
  TOKEN token;

  std::unordered_map<std::string_view, std::size_t> atom_umap;
  std::deque<std::string> atom_deq;

  STRICTNESS strictness;
  std::deque<STATEMENT> program;

  EXPECT<char32_t> next(int* advance);
  EXPECT<char32_t> prev(int* advance);
  std::string take(int* advance, int N);
  int backtrack(int* advance, int N);
  void skip_lf();

  EXPECT<char32_t> parse_hex(int* advance);

  EXPECT<char32_t> parse_hex(PARSE_ESCAPE);
  EXPECT<char32_t> parse_legacy_octal(PARSE_ESCAPE);
  EXPECT<char32_t> parse_uni_braced(PARSE_ESCAPE);
  EXPECT<char32_t> parse_uni_fixed(PARSE_ESCAPE);
  EXPECT<char32_t> parse_uni(PARSE_ESCAPE);
  bool parse_null(PARSE_ESCAPE);
  EXPECT<char32_t> parse(PARSE_ESCAPE);

  EXPECT<char32_t> parse_escape(PARSE_STRING);
  EXPECT<std::monostate> parse(PARSE_STRING);

  EXPECT<std::string> parse_uchar(PARSE_IDENT, bool beginning);
  EXPECT<std::monostate> parse(PARSE_IDENT);
  void update_token_ident();

  EXPECT<std::monostate> parse(PARSE_TOKEN);
  EXPECT<bool> parse_comment(PARSE_TOKEN, char32_t ahead);
  bool parse_comment_line(PARSE_TOKEN);
  EXPECT<bool> parse_comment_block(PARSE_TOKEN);
  EXPECT<bool> parse_iter(PARSE_TOKEN);

  TOKEN_KIND parse_init(PARSE_STMT);

  std::expected<std::size_t, std::monostate> find_static_atom();
  std::expected<std::size_t, std::monostate> find_dynamic_atom();

  bool parse();
};
}  // namespace Manadrain
