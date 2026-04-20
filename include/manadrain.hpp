#include <bitset>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "atom_page_neg1.hpp"

namespace Manadrain {
enum class ESC_RULE {
  IDENTIFIER,
  REGEXP_ASCII,
  REGEXP_UTF16,
  STRING_IN_STRICT_MODE,
  STRING_IN_TEMPLATE
};

enum class ESCAPE_ERR { MALFORMED, OCTAL_BANNED };
enum class NUMBER_ERR { INVALID_LITERAL, INTEGER_OVERFLOW };
enum class UNEXPECTED_ERR { STRING_END, COMMENT_END, THIS_TOKEN };
enum class NEEDED_ERR {
  COMMA,
  SEMICOLON,
  CLOSING_BRACE,
  CLOSING_BRACKET,
  VARIABLE_NAME,
  FIELD_NAME
};
using PARSE_ERRMSG =
    std::variant<ESCAPE_ERR, NUMBER_ERR, UNEXPECTED_ERR, NEEDED_ERR>;

struct TOK_STRING {
  char32_t separator;
  std::size_t p_atom;
  bool operator==(const TOK_STRING &) const = default;
};
struct TOK_IDENTI {
  bool has_escape;
  std::size_t p_atom;
  bool operator==(const TOK_IDENTI &) const = default;
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
enum class TOK_OPERATOR { EQ_STRICT, EQ_SLOPPY };
using TOKEN = std::variant<std::monostate, PARSE_ERRMSG, char32_t, TOK_STRING,
                           TOK_IDENTI, double, TOK_OPERATOR>;

struct EXPR_CALL;
struct EXPR_MEMBER;
struct EXPR_BINARY;
struct EXPR_OBJECT;
struct EXPR_ARRACCESS;
struct EXPR_ASSIGN;
constexpr int EXPRV_ERROR = 0;
using EXPRESSION = std::variant<PARSE_ERRMSG, TOK_STRING, TOK_IDENTI, double,
                                EXPR_CALL, EXPR_MEMBER, EXPR_BINARY,
                                EXPR_OBJECT, EXPR_ARRACCESS, EXPR_ASSIGN>;
using EXPR_PTR = std::shared_ptr<EXPRESSION>;
struct EXPR_CALL {
  EXPR_PTR callee;
  std::vector<EXPR_PTR> arguments;
};
struct EXPR_MEMBER {
  EXPR_PTR object;
  TOK_IDENTI property;
};
struct EXPR_BINARY {
  EXPR_PTR left;
  EXPR_PTR right;
  TOK_OPERATOR bin_op;
};
struct EXPR_ASSIGN {
  EXPR_PTR left;
  EXPR_PTR right;
};
struct EXPR_OBJECT {};
struct EXPR_ARRACCESS {
  EXPR_PTR object;
  EXPR_PTR property;
};

struct STMT_VARDECL {
  std::size_t p_kind;
  TOK_IDENTI identifier;
  EXPR_PTR initializer;
};
using STATEMENT = std::variant<STMT_VARDECL, EXPRESSION>;

struct PARSE_ESCAPE {
  ESC_RULE rule;
};
struct PARSE_STRING {};
struct PARSE_IDENT {};

constexpr std::uint8_t MEMORY_ALIGNMENT = 8;

struct ParseDriver {
  std::basic_string<std::uint8_t> buffer;
  int buffer_idx;
  bool reached_eof() { return buffer_idx >= buffer.size(); }

  bool newline_seen;

  std::string str1_temp;
  void str1_encode(char32_t cp);

  std::unordered_map<std::string, std::size_t> atom_umap;
  std::vector<char> mach_mem;

  TOKEN token_curr;
  std::vector<STATEMENT> program;

  std::optional<char32_t> next();
  std::optional<char32_t> peek();
  void prev();
  void backtrack(std::size_t N);

  void skip_lf();

  bool skip_comment_line();
  std::expected<bool, PARSE_ERRMSG> skip_comment_block();
  std::expected<bool, PARSE_ERRMSG> skip_comment(char32_t ch);
  std::expected<bool, PARSE_ERRMSG> skip_ws_1(char32_t ch);

  std::size_t find_atom();
  std::size_t alloc_atom();

  char32_t parse_octo(PARSE_ESCAPE, char32_t oct);
  std::expected<char32_t, PARSE_ERRMSG> parse_hex(PARSE_ESCAPE);
  std::optional<std::expected<char32_t, PARSE_ERRMSG>> parse(PARSE_ESCAPE esc,
                                                             char32_t ch);

  std::optional<std::expected<char32_t, PARSE_ERRMSG>>
  parse_escape(PARSE_STRING, char32_t separator, char32_t ch);
  std::optional<std::expected<char32_t, PARSE_ERRMSG>>
  parse_uchar(PARSE_STRING, char32_t separator, char32_t ch);
  std::expected<void, PARSE_ERRMSG> parse_atom(PARSE_STRING,
                                               char32_t separator);

  std::optional<char32_t> parse_uni_braced(PARSE_IDENT);
  std::optional<char32_t> parse_uni_fixed(PARSE_IDENT, char32_t leading);
  std::optional<char32_t> parse_uni(PARSE_IDENT);
  bool parse_uchar(PARSE_IDENT, char32_t ch);
  std::expected<int, PARSE_ERRMSG> parse_escape(PARSE_IDENT, char32_t leading);
  std::optional<std::expected<TOK_IDENTI, PARSE_ERRMSG>> parse(PARSE_IDENT);

  TOKEN tokenize();
  std::optional<TOKEN> tokenize_lookahead(char32_t leading);
  std::optional<TOKEN> tokenize_identi_or_punct();

  EXPR_PTR parse_assign_expr();
  EXPR_PTR parse_binary_expr();
  EXPR_PTR parse_postfix_expr();
  std::pair<bool, EXPR_PTR> parse_postfix_expr(EXPR_PTR expression);
  EXPRESSION parse_primary_expr(char32_t punct);
  EXPRESSION parse_primary_expr();
  std::pair<bool, EXPR_PTR> parse_arg_expr();
  EXPR_PTR parse_call_expr(EXPR_PTR callee);
  EXPR_PTR parse_member_expr(EXPR_PTR object);
  EXPR_PTR parse_array_access(EXPR_PTR object);

  std::variant<std::monostate, STMT_VARDECL, PARSE_ERRMSG>
  parse_variable_decl();
  std::expected<void, PARSE_ERRMSG> parse_statement();
  std::expected<void, PARSE_ERRMSG> expect_statement_end();

  bool parse();
};
} // namespace Manadrain
