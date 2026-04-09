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
  enum ERRCODE { OUT_OF_RANGE };
};
struct PARSE_HEX {
  enum ERRCODE { NOT_A_DIGIT = MOVE_BUFIDX::OUT_OF_RANGE + 1 };
};
struct PARSE_ESCAPE {
  enum ERRCODE {
    MALFORMED = PARSE_HEX::NOT_A_DIGIT + 1,
    OCTAL_SEQ,
    PER_SE_BACKSLASH
  };
  ESC_RULE rule;
};
struct PARSE_STRING {
  enum ERRCODE {
    UNEXPECTED_END = PARSE_ESCAPE::PER_SE_BACKSLASH + 1,
    OCTAL_SEQ,
    MALFORMED_ESC,
    MUST_CONTINUE
  };
};
struct PARSE_COMMENT {
  enum ERRCODE { UNEXPECTED_END = PARSE_STRING::MUST_CONTINUE + 1 };
};

struct ParseDriver {
  std::basic_string<std::uint8_t> buffer;
  std::int32_t buffer_idx;

  int fwd_cnt;
  void reset_fwd() { fwd_cnt = 0; }

  std::string ch_temp;
  TOKEN token;

  std::unordered_map<std::string_view, std::size_t> atom_umap;
  std::deque<std::string> atom_deq;
  std::size_t get_atom();

  STRICTNESS strictness;
  std::deque<STATEMENT> program;

  std::expected<char32_t, int> next();
  std::expected<char32_t, int> prev();
  std::string take(int* actual, int N);
  int backtrack(int N);

  std::expected<std::uint32_t, int> parse_hex();

  std::expected<char32_t, int> parse_hex(PARSE_ESCAPE);
  std::expected<char32_t, int> parse_uni_braced(PARSE_ESCAPE);
  std::expected<char32_t, int> parse_uni_fixed(PARSE_ESCAPE);
  std::expected<char32_t, int> parse_uni(PARSE_ESCAPE);
  std::expected<char32_t, int> parse(PARSE_ESCAPE);

  std::expected<char32_t, int> parse_escape(PARSE_STRING);

  bool parse();
};
}  // namespace Manadrain
