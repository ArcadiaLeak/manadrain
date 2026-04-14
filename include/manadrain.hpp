#include <bitset>
#include <deque>
#include <expected>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "atom_page_neg1.hpp"

namespace Manadrain {
enum class STRICTNESS { SLOPPY, STRICT };
enum class KEYWORD_KIND { K_LET, K_CONST, K_VAR };
enum class ESC_RULE {
  IDENTIFIER,
  REGEXP_ASCII,
  REGEXP_UTF16,
  STRING_IN_SLOPPY_MODE,
  STRING_IN_STRICT_MODE,
  STRING_IN_TEMPLATE
};
enum class PARSE_ERRCODE {
  MALFORMED_ESCAPE,
  LEGACY_OCTAL_SEQ,
  UNEXPECTED_STRING_END,
  UNEXPECTED_COMMENT_END,
  UNEXPECTED_TOKEN,
  NEEDED_VARIABLE_NAME,
  NEEDED_SPECIFICLY
};

struct TOK_STRING {
  char32_t separator;
  P_ATOM p_atom;
};
struct TOK_IDENTI {
  bool has_escape;
  P_ATOM p_atom;
  std::optional<KEYWORD_KIND> match_keyword(STRICTNESS);
};
using TOKEN =
    std::expected<std::variant<char32_t, TOK_STRING, TOK_IDENTI>, int>;
using EXPRESSION = std::variant<TOK_STRING, TOK_IDENTI>;

struct STMT_VARDECL {
  KEYWORD_KIND category;
  TOK_IDENTI identifier;
  EXPRESSION initializer;
};
using STATEMENT = std::variant<STMT_VARDECL, EXPRESSION>;

struct PARSE_ESCAPE {
  ESC_RULE rule;
};
struct PARSE_STRING {
  static constexpr int flag = 1;
};
struct PARSE_IDENT {
  static constexpr int flag = 2;
  bool is_private;
};
struct PARSE_PUNCT {
  static constexpr int flag = 4;
};
struct PARSE_STATEMENT {};
struct PARSE_VARDECL {};
struct PARSE_POSTFIX_EXPR {};

constexpr std::uint8_t ATOM_BLOCK = 8;
constexpr std::uint16_t ATOM_PAGE = 2048;

struct AtomPage {
  std::bitset<ATOM_PAGE> ch_bitset;
  std::array<char, ATOM_BLOCK * ATOM_PAGE> ch_arr;
  bool check_for_count(int N);
  bool check_for_window(int N);
  int scan_for_window(int N);
  std::optional<std::uint16_t> try_allocate(int N);
  void allocate(int offset, int N);
};

struct ParseDriver {
  std::basic_string<std::uint8_t> buffer;
  int buffer_idx;
  bool reached_eof() { return buffer_idx >= buffer.size(); }

  bool newline_seen;
  std::optional<PARSE_ERRCODE> errcode;

  std::string str1_temp;
  void str1_encode(char32_t cp);

  std::unordered_map<std::string_view, P_ATOM> atom_umap;
  std::deque<AtomPage> atom_deq;

  STRICTNESS strictness;
  std::vector<STATEMENT> program;

  char32_t next();
  void prev();
  void backtrack(std::size_t N);

  void skip_lf();

  bool skip_comment_line();
  std::optional<bool> skip_comment_block();
  std::optional<bool> skip_comment(char32_t ch);
  std::optional<bool> skip_ws_1(char32_t ch);

  std::optional<P_ATOM> find_static_atom();
  std::optional<P_ATOM> find_dynamic_atom();
  P_ATOM alloc_dynamic_atom();

  char32_t parse_octo(PARSE_ESCAPE, char32_t oct);
  std::expected<char32_t, int> parse(PARSE_ESCAPE esc, char32_t ch);

  std::expected<char32_t, int> parse_escape(PARSE_STRING, char32_t separator,
                                            char32_t ch);
  std::expected<char32_t, int> parse_uchar(PARSE_STRING, char32_t separator,
                                           char32_t ch);
  std::optional<std::monostate> parse_atom(PARSE_STRING, char32_t separator);

  bool is_allowed_uchar(PARSE_IDENT ident, char32_t ch);
  std::optional<bool> parse_uchar(PARSE_IDENT ident);
  std::optional<bool> parse_atom(PARSE_IDENT ident);

  TOKEN tokenize(int flags);
  std::optional<EXPRESSION> parse(PARSE_POSTFIX_EXPR);
  
  std::optional<KEYWORD_KIND> parse_beginning(PARSE_STATEMENT);
  std::optional<std::monostate> parse(PARSE_VARDECL, std::size_t idx);

  bool parse();
};
} // namespace Manadrain
