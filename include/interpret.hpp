#include <expected>
#include <generator>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <stack>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "atom_prealloc.hpp"

namespace Interpret {
enum class INVALID_ERR { NUMBER_LITERAL, PROPERTY_NAME, MALFORMED_ESCAPE };
enum class UNEXPECTED_ERR { STRING_END, COMMENT_END, THIS_TOKEN };
enum class NEEDED_ERR { FIELD_NAME, VARIABLE_NAME, FUNCTION_NAME };
struct PUNCT_ERR {
  char32_t must_be;
};
using PARSE_ERRMSG =
    std::variant<INVALID_ERR, UNEXPECTED_ERR, NEEDED_ERR, PUNCT_ERR>;

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
  TOKV_PUNCT,
  TOKV_STRING,
  TOKV_IDENTI,
  TOKV_NUMBER,
  TOKV_OP
};
enum class TOK_OPERATOR {
  EQ_STRICT,
  EQ_SLOPPY,
  DIV_ASSIGN,
  AND_ASSIGN,
  LAND_ASSIGN,
  LOGIC_AND
};
using TOKEN = std::variant<std::monostate, char32_t, TOK_STRING, TOK_IDENTI,
                           double, TOK_OPERATOR>;

struct EXPR_CALL;
struct EXPR_MEMBER;
struct EXPR_BINARY;
struct EXPR_OBJECT;
struct EXPR_ACCESS;
struct EXPR_ASSIGN;
struct EXPR_LOGICAL;
using EXPRESSION =
    std::variant<std::monostate, TOK_STRING, TOK_IDENTI, double,
                 std::unique_ptr<EXPR_CALL>, std::unique_ptr<EXPR_MEMBER>,
                 std::unique_ptr<EXPR_BINARY>, std::unique_ptr<EXPR_OBJECT>,
                 std::unique_ptr<EXPR_ACCESS>, std::unique_ptr<EXPR_ASSIGN>,
                 std::unique_ptr<EXPR_LOGICAL>>;
struct EXPR_CALL {
  EXPRESSION callee;
  std::vector<EXPRESSION> arguments;
};
struct EXPR_MEMBER {
  EXPRESSION object;
  TOK_IDENTI property;
};
struct EXPR_BINARY {
  EXPRESSION left;
  EXPRESSION right;
  TOKEN op;
};
struct DECL_FUNCTION;
struct EXPR_OBJECT {
  struct KEY_VALUE {
    EXPRESSION prop_key;
    EXPRESSION prop_val;
  };
  using PROPERTY = std::variant<KEY_VALUE, DECL_FUNCTION>;
  std::vector<PROPERTY> props;
};
struct EXPR_ACCESS {
  EXPRESSION object;
  EXPRESSION property;
};
struct EXPR_ASSIGN {
  EXPRESSION left;
  EXPRESSION right;
};
struct EXPR_LOGICAL {
  EXPRESSION left;
  EXPRESSION right;
  TOKEN op;
};

struct DECL_VARIABLE {
  std::size_t p_kind;
  TOK_IDENTI identifier;
  EXPRESSION initializer;
};
struct STMT_RETURN {
  EXPRESSION argument;
};
using STATEMENT =
    std::variant<DECL_VARIABLE, EXPRESSION, DECL_FUNCTION, STMT_RETURN>;

struct DECL_FUNCTION {
  TOK_IDENTI identifier;
  std::vector<STATEMENT> subprogram;
};

constexpr std::uint8_t MEMORY_ALIGNMENT = 8;

class Scanner {
public:
  void populate(const std::vector<std::uint8_t> &buffer_ref) {
    buffer = buffer_ref;
  }
  void populate(std::vector<std::uint8_t> &&buffer_ref) {
    buffer = std::move(buffer_ref);
  }

protected:
  bool reached_end();
  void prev();
  char32_t unchecked_next();
  std::optional<char32_t> next();
  std::optional<char32_t> peek();
  void backtrack(std::size_t N);
  void chewLF();

private:
  int position;
  std::vector<std::uint8_t> buffer;
  std::stack<int> backtrace;
};

enum class TOK_0PREFIX { ZERO_X, ZERO_B, ZERO_O, ZERO };
class TokNumber : public Scanner {
protected:
  std::expected<TOKEN, PARSE_ERRMSG> tokenize(char32_t leading);

private:
  struct WHOLE {
    std::string repr_s;
    std::string collapse() { return repr_s; }
  };
  struct FRACTIONAL {
    WHOLE whole;
    std::string frac_s;
    std::string collapse();
  };
  struct SCIENTIFIC {
    std::variant<WHOLE, FRACTIONAL> expon_base;
    std::string expon_s;
    std::string collapse() { return {}; }
  };
  using FLOAT_REPR = std::variant<WHOLE, FRACTIONAL, SCIENTIFIC>;

  std::optional<TOK_0PREFIX> decode_0prefix();
  std::string scan_numseq(std::optional<TOK_0PREFIX> base_opt,
                          std::optional<char32_t> ahead);
};

class TokAtom : public TokNumber {
private:
  std::unordered_map<std::string, std::size_t> atom_umap;
  std::vector<char> mempool{std::from_range, atom_prealloc_buf};

  std::size_t atom_find(std::string needle);
  std::size_t atom_alloc(std::string_view needle);

  std::optional<char32_t> decode_string_esc8();
  std::optional<char32_t> decode_string_xseq();
  std::optional<char32_t> decode_string_uni();

  std::generator<std::expected<char32_t, PARSE_ERRMSG>>
  traverse_string(char32_t separator);
  std::generator<std::optional<char32_t>> traverse_identif(bool &has_escape);

protected:
  std::expected<TOKEN, PARSE_ERRMSG> tokenize_string(char32_t separator);
  std::expected<TOKEN, PARSE_ERRMSG> tokenize_identif(char32_t leading);
  std::optional<char32_t> decode_identif_uni();
};

class Tokenizer : public TokAtom {
protected:
  std::expected<TOKEN, PARSE_ERRMSG> tokenize();
  bool newlineSeen() { return newline_seen; }

private:
  bool newline_seen;
};

enum class PROP_KIND { IDENTIF, COMPUTED };

class Parser : public Tokenizer {
public:
  std::vector<STATEMENT> program;
  bool parse();

private:
  TOKEN my_token;
  EXPRESSION my_expression;

  std::expected<void, PARSE_ERRMSG> tokenize();
  std::expected<void, PARSE_ERRMSG> expect_statement_end();
  std::expected<void, PARSE_ERRMSG> expect_punct(char32_t punct);

  std::expected<void, PARSE_ERRMSG> parse_assign_expr();
  std::expected<void, PARSE_ERRMSG> parse_binary_prec5();
  std::expected<void, PARSE_ERRMSG> parse_binary_prec4();
  std::expected<void, PARSE_ERRMSG> parse_postfix_expr();
  std::expected<void, PARSE_ERRMSG> parse_primary_expr();
  std::expected<void, PARSE_ERRMSG> parse_call_expr();
  std::expected<void, PARSE_ERRMSG> parse_member_expr();
  std::expected<void, PARSE_ERRMSG> parse_access_expr();
  std::expected<void, PARSE_ERRMSG> parse_object_literal();
  std::expected<void, PARSE_ERRMSG> parse_logical_and_or();

  std::expected<void, PARSE_ERRMSG> parse_variable_decl();
  std::expected<DECL_FUNCTION, PARSE_ERRMSG>
  parse_function_decl(TOK_IDENTI identifier);
  std::expected<void, PARSE_ERRMSG> parse_statement();
};
} // namespace Interpret
