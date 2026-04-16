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

enum class ESCAPE_ERR { MALFORMED, OCTAL_BANNED };
enum class NUMBER_ERR { INVALID_LITERAL, INTEGER_OVERFLOW };
enum class UNEXPECTED_ERR { STRING_END, COMMENT_END, THIS_TOKEN };
enum class NEEDED_ERR { COMMA, SEMICOLON, VARIABLE_NAME, FIELD_NAME };
using PARSE_ERRMSG =
    std::variant<ESCAPE_ERR, NUMBER_ERR, UNEXPECTED_ERR, NEEDED_ERR>;

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
enum TOKV_INDEX {
  TOKV_EOF,
  TOKV_ERROR,
  TOKV_PUNCT,
  TOKV_STRING,
  TOKV_IDENTI,
  TOKV_NUMBER,
  TOKV_OP
};
enum TOK_OPERATOR { EQ_STRICT, EQ_LOOSE };
using TOKEN = std::variant<std::monostate, PARSE_ERRMSG, char32_t, TOK_STRING,
                           TOK_IDENTI, double, TOK_OPERATOR>;

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
  std::variant<TOK_STRING, TOK_IDENTI, double, EXPR_CALL, EXPR_MEMBER> alter;
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
struct PARSE_IDENT {};

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
  STRICTNESS strictness;

  std::string str1_temp;
  void str1_encode(char32_t cp);

  std::unordered_map<std::string_view, P_ATOM> atom_umap;
  std::deque<AtomPage> atom_deq;

  TOKEN token_curr;
  std::vector<STATEMENT> program;

  char32_t next();
  std::optional<char32_t> peek();
  void prev();
  void backtrack(std::size_t N);

  void skip_lf();

  bool skip_comment_line();
  std::variant<bool, PARSE_ERRMSG> skip_comment_block();
  std::variant<bool, PARSE_ERRMSG> skip_comment(char32_t ch);
  std::variant<bool, PARSE_ERRMSG> skip_ws_1(char32_t ch);

  std::optional<P_ATOM> find_static_atom();
  std::optional<P_ATOM> find_dynamic_atom();
  P_ATOM alloc_dynamic_atom();

  char32_t parse_octo(PARSE_ESCAPE, char32_t oct);
  std::variant<std::monostate, PARSE_ERRMSG, char32_t> parse(PARSE_ESCAPE esc,
                                                             char32_t ch);

  std::variant<std::monostate, PARSE_ERRMSG, char32_t>
  parse_escape(PARSE_STRING, char32_t separator, char32_t ch);
  std::variant<std::monostate, PARSE_ERRMSG, char32_t>
  parse_uchar(PARSE_STRING, char32_t separator, char32_t ch);
  std::variant<std::monostate, PARSE_ERRMSG> parse_atom(PARSE_STRING,
                                                        char32_t separator);

  int parse_uni_fixed(PARSE_IDENT, char32_t leading);
  int parse_uni(PARSE_IDENT);
  bool parse_uchar(PARSE_IDENT, char32_t ch);
  std::variant<int, PARSE_ERRMSG> parse_escape(PARSE_IDENT, char32_t leading);
  std::optional<std::variant<TOK_IDENTI, PARSE_ERRMSG>> parse(PARSE_IDENT);

  TOKEN tokenize();
  std::optional<TOKEN> tokenize_lookahead(char32_t leading);
  std::optional<TOKEN> tokenize_identi_or_punct();

  std::variant<EXPRESSION, PARSE_ERRMSG> parse_postfix_expr();

  std::variant<std::monostate, PARSE_ERRMSG>
  parse_variable_decl(std::size_t idx);

  bool parse();
};
} // namespace Manadrain
