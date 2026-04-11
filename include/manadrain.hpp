#include <array>
#include <cstdint>
#include <deque>
#include <expected>
#include <optional>
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
  bool newline_seen;
  std::variant<char32_t, PAYLOAD_STR, PAYLOAD_IDENT> data;
};

struct EXPR_IDENT {
  std::size_t atom_idx;
};
using EXPRESSION = std::variant<EXPR_IDENT>;

struct STMT_VARDECL {
  TOKEN_KIND intro;
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

enum class PARSE_ERRCODE {
  OUT_OF_RANGE,
  MALFORMED_ESCAPE,
  LEGACY_OCTAL_SEQ,
  STRING_UNEXPECTED_END,
  COMMENT_UNEXPECTED_END,
  VARIABLE_NAME_EXPECTED
};

struct PARSE_ESCAPE {
  ESC_RULE rule;
};
struct PARSE_STRING {};
struct PARSE_TOKEN {};
struct PARSE_IDENT {
  bool is_private;
};
struct PARSE_STATEMENT {};
struct PARSE_VARDECL {};
struct PARSE_POSTFIX_EXPR {};

struct ENCODED_POINT {
  std::array<char, 4> buff;
  std::uint16_t length;
  std::string_view sv() const { return {buff.data(), length}; }
};
ENCODED_POINT codepoint_cv(char32_t ch);

template <typename T> using EXPECT = std::expected<T, PARSE_ERRCODE>;
template <typename T>
using EXPECT_OPT = std::expected<std::optional<T>, PARSE_ERRCODE>;

struct ParseDriver {
  std::basic_string<std::uint8_t> buffer;
  std::int32_t buffer_idx;

  int mov_since_mark;
  int mov_in_take;

  std::string str0_temp;
  std::string str1_temp;

  std::unordered_map<std::string_view, std::size_t> atom_umap;
  std::deque<std::string> atom_deq;

  STRICTNESS strictness;
  TOKEN token;
  EXPRESSION expression;
  std::deque<STATEMENT> program;

  EXPECT<char32_t> next();
  EXPECT<char32_t> prev();
  std::string_view take(int N);
  int backtrack(int N);
  void skip_lf();

  std::optional<std::size_t> find_static_atom();
  std::optional<std::size_t> find_dynamic_atom();

  EXPECT<char32_t> parse_hex();

  EXPECT<char32_t> parse_hex(PARSE_ESCAPE);
  char32_t parse_oct_digit(PARSE_ESCAPE, char32_t oct);
  EXPECT<char32_t> parse_uni_braced(PARSE_ESCAPE);
  EXPECT<char32_t> parse_uni_fixed(PARSE_ESCAPE);
  EXPECT<char32_t> parse_uni(PARSE_ESCAPE);
  bool parse_null(PARSE_ESCAPE);
  EXPECT_OPT<char32_t> parse(PARSE_ESCAPE, char32_t ch);

  EXPECT_OPT<char32_t> parse_escape(PARSE_STRING, char32_t ch);
  EXPECT_OPT<char32_t> parse_uchar(PARSE_STRING, char32_t ch);
  EXPECT<void> parse(PARSE_STRING);

  EXPECT<ENCODED_POINT> parse_uchar(PARSE_IDENT, bool beginning);
  EXPECT<bool> parse(PARSE_IDENT);
  void update_token_ident();

  EXPECT<void> parse(PARSE_TOKEN);
  EXPECT<bool> parse_comment(PARSE_TOKEN, char32_t ahead);
  bool parse_comment_line(PARSE_TOKEN);
  EXPECT<bool> parse_comment_block(PARSE_TOKEN);
  EXPECT<bool> parse_iter(PARSE_TOKEN);

  EXPECT<TOKEN_KIND> parse_init(PARSE_STATEMENT);

  EXPECT<void> parse(PARSE_VARDECL);
  EXPECT<void> parse(PARSE_POSTFIX_EXPR);

  bool parse();
};
} // namespace Manadrain
