#include <array>
#include <cstdint>
#include <deque>
#include <expected>
#include <string>
#include <unordered_map>
#include <variant>

namespace Manadrain {
enum class STRICTNESS { SLOPPY, STRICT };

constexpr int K_TOKEN_EOF = 0;
constexpr int K_TOKEN_LET = 1;
constexpr int K_TOKEN_CONST = 2;
constexpr int K_TOKEN_VAR = 3;
constexpr int K_TOKEN_IDENT = 4;
constexpr int K_TOKEN_STRING = 5;
constexpr int K_TOKEN_ERROR = 6;
constexpr int K_TOKEN_UCHAR = 7;

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

  int kind;
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
  static constexpr int UNEXPECTED_END = 0;
  static constexpr int ILLEGAL_UTF8 = 1;
};
struct PARSE_HEX {
  static constexpr int ILLEGAL_DIGIT = 2;
};
struct PARSE_ESCAPE {
  ESC_RULE rule;
  static constexpr int MALFORMED = 3;
  static constexpr int OCTAL_SEQ = 4;
  static constexpr int PER_SE_BACKSLASH = 5;
};
struct PARSE_STRING {
  static constexpr int UNEXPECTED_END = 6;
  static constexpr int OCTAL_SEQ = 7;
  static constexpr int MALFORMED_ESC = 8;
  static constexpr int MUST_CONTINUE = 9;
};
struct PARSE_COMMENT {
  static constexpr int UNEXPECTED_END = 10;
};

struct ParseDriver {
  std::basic_string<std::uint8_t> buffer;
  std::uint32_t buffer_idx;

  std::uint32_t fwd_cnt;
  void reset_fwd() { fwd_cnt = 0; }

  STRICTNESS strictness;
  TOKEN token;

  std::deque<STATEMENT> program;
  std::unordered_map<std::string_view, std::size_t> atom_umap;
  std::deque<std::string> atom_deq;
  std::size_t get_atom();

  std::string ch_temp;
  std::string_view take(std::uint32_t N);

  std::expected<char32_t, int> peek();
  void forward(std::uint32_t count);
  void backtrack(std::uint32_t count);

  std::expected<std::uint32_t, int> parse_hex(char32_t uchar);
  std::expected<std::uint32_t, int> parse_hex();

  std::expected<char32_t, int> parse_hex(PARSE_ESCAPE);
  std::expected<char32_t, int> parse_uni_braced(PARSE_ESCAPE);
  std::expected<char32_t, int> parse_uni_fixed(PARSE_ESCAPE);
  std::expected<char32_t, int> parse_uni(PARSE_ESCAPE);
  std::expected<char32_t, int> parse(PARSE_ESCAPE);

  std::expected<char32_t, int> parse_escape(TOKEN::PAYLOAD_STR& payload);

  bool parse();
};
}  // namespace Manadrain
