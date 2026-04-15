#include <bitset>
#include <deque>
#include <memory>
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
  NEEDED_COMMA,
  NEEDED_SEMICOLON,
  NEEDED_VARIABLE_NAME,
  NEEDED_FIELD_NAME
};

struct TOK_STRING {
  char32_t separator;
  P_ATOM p_atom;
  bool operator==(const TOK_STRING &) const = default;
};
struct TOK_IDENTI {
  bool has_escape;
  P_ATOM p_atom;
  bool operator==(const TOK_IDENTI &) const = default;
  std::optional<KEYWORD_KIND> match_keyword(STRICTNESS);
};
enum TOKV_INDEX { TOKV_EOF, TOKV_ERROR, TOKV_PUNCT, TOKV_STRING, TOKV_IDENTI };
using TOKEN = std::variant<std::monostate, PARSE_ERRCODE, char32_t, TOK_STRING,
                           TOK_IDENTI>;

struct EXPRESSION;

struct EXPR_CALL {
  std::unique_ptr<EXPRESSION> callee;
  std::vector<EXPRESSION> arguments;
};
struct EXPR_MEMBER {
  std::unique_ptr<EXPRESSION> object;
  TOK_IDENTI property;
};

struct EXPRESSION {
  enum V_INDEX { V_STRING, V_IDENTI, V_CALL, V_MEMBER };
  std::variant<TOK_STRING, TOK_IDENTI, EXPR_CALL, EXPR_MEMBER> alter;
};

struct STMT_VARDECL {
  KEYWORD_KIND rule;
  TOK_IDENTI identifier;
  std::optional<EXPRESSION> initializer;
};
using STATEMENT = std::variant<STMT_VARDECL, EXPRESSION>;

struct PARSE_ESCAPE {
  ESC_RULE rule;
};
struct PARSE_STRING {};
struct PARSE_IDENT {
  bool is_private;
};
struct PARSE_EOF {};
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

  std::string str1_temp;
  void str1_encode(char32_t cp);

  std::unordered_map<std::string_view, P_ATOM> atom_umap;
  std::deque<AtomPage> atom_deq;

  TOKEN token_curr;

  STRICTNESS strictness;
  std::vector<STATEMENT> program;

  char32_t next();
  void prev();
  void backtrack(std::size_t N);

  void skip_lf();

  bool skip_comment_line();
  std::variant<bool, PARSE_ERRCODE> skip_comment_block();
  std::variant<bool, PARSE_ERRCODE> skip_comment(char32_t ch);
  std::variant<bool, PARSE_ERRCODE> skip_ws_1(char32_t ch);

  std::optional<P_ATOM> find_static_atom();
  std::optional<P_ATOM> find_dynamic_atom();
  P_ATOM alloc_dynamic_atom();

  char32_t parse_octo(PARSE_ESCAPE, char32_t oct);
  std::variant<std::monostate, PARSE_ERRCODE, char32_t> parse(PARSE_ESCAPE esc,
                                                              char32_t ch);

  std::variant<std::monostate, PARSE_ERRCODE, char32_t>
  parse_escape(PARSE_STRING, char32_t separator, char32_t ch);
  std::variant<std::monostate, PARSE_ERRCODE, char32_t>
  parse_uchar(PARSE_STRING, char32_t separator, char32_t ch);
  std::variant<std::monostate, PARSE_ERRCODE> parse_atom(PARSE_STRING,
                                                         char32_t separator);

  bool is_allowed_uchar(PARSE_IDENT ident, char32_t ch);
  std::variant<bool, PARSE_ERRCODE> parse_uchar(PARSE_IDENT ident);
  std::variant<bool, PARSE_ERRCODE> parse_atom(PARSE_IDENT ident);

  TOKEN tokenize();
  std::variant<EXPRESSION, PARSE_ERRCODE> parse(PARSE_POSTFIX_EXPR);

  std::variant<std::monostate, PARSE_ERRCODE> parse(PARSE_VARDECL,
                                                    std::size_t idx);

  bool parse();
};
} // namespace Manadrain
