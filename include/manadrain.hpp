#include <array>
#include <bitset>
#include <cstdint>
#include <deque>
#include <expected>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

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
    char32_t separator;
    std::size_t atom_idx;
  };
  struct PAYLOAD_IDENT {
    bool has_escape;
    bool is_reserved;
    std::size_t atom_idx;
  };
  using PAYLOAD_ERR = std::variant<char32_t>;
  TOKEN_KIND kind;
  bool newline_seen;
  std::variant<char32_t, PAYLOAD_STR, PAYLOAD_IDENT, PAYLOAD_ERR> data;
};

using EXPRESSION = std::variant<TOKEN::PAYLOAD_IDENT, TOKEN::PAYLOAD_STR>;

struct STMT_VARDECL {
  TOKEN_KIND intro;
  TOKEN::PAYLOAD_IDENT identifier;
  EXPRESSION initializer;
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
  UNEXPECTED_STRING_END,
  UNEXPECTED_COMMENT_END,
  UNEXPECTED_TOKEN,
  NEEDED_VARIABLE_NAME,
  NEEDED_SPECIFICLY
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

template <typename T> using EXPECT = std::expected<T, PARSE_ERRCODE>;
template <typename T>
using EXPECT_OPT = std::expected<std::optional<T>, PARSE_ERRCODE>;

struct ParseDriver {
  std::basic_string<std::uint8_t> buffer;
  std::int32_t buffer_idx;

  int mov_since_mark;

  std::string str1_temp;
  std::u32string str4_temp;
  void codepoint_cv(char32_t cp);

  std::unordered_map<std::string_view, std::size_t> atom_umap;
  std::deque<std::string> atom_deq;
  // std::deque<std::pair<std::bitset<4096>, std::array<char, 8 * 4096>>> atom_deq;

  STRICTNESS strictness;
  TOKEN token;
  std::vector<STATEMENT> program;

  EXPECT<char32_t> next();
  EXPECT<char32_t> prev();
  std::u32string_view take(int N);
  std::size_t backtrack(std::size_t N);
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
  bool should_append(PARSE_STRING, char32_t ch);
  EXPECT<void> parse(PARSE_STRING);

  bool is_allowed_uchar(PARSE_IDENT ident, char32_t ch);
  EXPECT<bool> parse_uchar(PARSE_IDENT);
  EXPECT<bool> parse(PARSE_IDENT);
  void update_token_ident();

  EXPECT<void> parse(PARSE_TOKEN);
  EXPECT<bool> parse_comment(PARSE_TOKEN, char32_t ahead);
  bool parse_comment_line(PARSE_TOKEN);
  EXPECT<bool> parse_comment_block(PARSE_TOKEN);
  EXPECT<bool> parse_iter(PARSE_TOKEN);

  EXPECT<TOKEN_KIND> parse_init(PARSE_STATEMENT);

  EXPECT<void> parse(PARSE_VARDECL);
  EXPECT<void> parse(PARSE_POSTFIX_EXPR, EXPRESSION &expression);

  bool parse();
};
} // namespace Manadrain
