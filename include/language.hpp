#include <gmpxx.h>

#include <deque>
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

#include "expected_task.hpp"

namespace Manadrain {
namespace Language {
enum class INVALID_ERR {
  NUMBER_LITERAL,
  BIGINT_LITERAL,
  PROPERTY_NAME,
  BACKSLASH_ESCAPE
};
enum class UNEXPECT_ERR { STRING_END, COMMENT_END, THIS_TOKEN };
enum class REQUIRED_ERR {
  FIELD_NAME,
  VARIABLE_NAME,
  FUNCTION_NAME,
  IDENTIFIER,
  FROM_CLAUSE,
  STRING_LITERAL,
  FORMAL_PARAMETER
};
struct KEYWORD_ERR {
  std::size_t atom_sh;
};
struct PUNCT_ERR {
  char32_t must_be;
};
using PARSE_ERRMSG = std::variant<INVALID_ERR, UNEXPECT_ERR, REQUIRED_ERR,
                                  PUNCT_ERR, KEYWORD_ERR>;

struct TOK_STRING {
  char32_t separator;
  std::size_t atom_sh;
  bool operator==(const TOK_STRING &) const = default;
};
struct TOK_IDENTI {
  bool has_escape;
  std::size_t atom_sh;
  bool operator==(const TOK_IDENTI &) const = default;
};
struct TOK_BIGINT {
  std::size_t idx;
  bool operator==(const TOK_BIGINT &) const = default;
};
enum TOKV_INDEX {
  TOKV_EOF,
  TOKV_PUNCT,
  TOKV_STRING,
  TOKV_IDENTI,
  TOKV_NUMBER,
  TOKV_OP,
  TOKV_BIGINT
};
enum class TOK_OPERATOR {
  DOUBLE_EQUALS,
  TRIPLE_EQUALS,
  BANG_EQUALS,
  BANG_DOUBLE_EQUALS,
  DIVIDE_ASSIGN,
  BITWISE_CONJUNCT_ASSIGN,
  LOGICAL_CONJUNCT_ASSIGN,
  LOGICAL_CONJUNCT,
  BITWISE_DISJUNCT_ASSIGN,
  LOGICAL_DISJUNCT_ASSIGN,
  LOGICAL_DISJUNCT
};
using TOKEN = std::variant<std::monostate, char32_t, TOK_STRING, TOK_IDENTI,
                           double, TOK_OPERATOR, TOK_BIGINT>;

struct EXPR_CALL;
struct EXPR_MEMBER;
struct EXPR_BINARY;
struct EXPR_OBJECT;
struct EXPR_ACCESS;
struct EXPR_ASSIGN;
struct EXPR_LOGICAL;
struct DECL_FUNCTION;
using EXPRESSION =
    std::variant<std::monostate, TOK_STRING, TOK_IDENTI, double,
                 std::unique_ptr<EXPR_CALL>, std::unique_ptr<EXPR_MEMBER>,
                 std::unique_ptr<EXPR_BINARY>, std::unique_ptr<EXPR_OBJECT>,
                 std::unique_ptr<EXPR_ACCESS>, std::unique_ptr<EXPR_ASSIGN>,
                 std::unique_ptr<EXPR_LOGICAL>, std::unique_ptr<DECL_FUNCTION>>;
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
  std::size_t kind;
  TOK_IDENTI identifier;
  EXPRESSION initializer;
};
struct DECL_IMPORT {
  std::vector<TOK_IDENTI> specifiers;
  TOK_STRING source;
};
struct STMT_RETURN {
  EXPRESSION argument;
};
struct STMT_BLOCK;
struct STMT_IF;
using STATEMENT = std::variant<DECL_VARIABLE, EXPRESSION, DECL_FUNCTION,
                               STMT_RETURN, DECL_IMPORT, STMT_BLOCK, STMT_IF>;
struct STMT_BLOCK {
  std::vector<STATEMENT> subprogram;
};
struct STMT_IF {
  EXPRESSION condition;
  std::unique_ptr<STATEMENT> consequent;
  std::unique_ptr<STATEMENT> alternate;
};
struct DECL_FUNCTION {
  EXPRESSION identifier;
  std::vector<std::size_t> arguments;
  std::vector<STATEMENT> subprogram;
};

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
  void skip_lf();
  void skip_shebang();

private:
  std::size_t position;
  std::vector<std::uint8_t> buffer;
  std::stack<int> backtrace;
};

enum class TOK_0PREFIX { ZERO_X, ZERO_B, ZERO_O, ZERO };
class TokNumber : public Scanner {
protected:
  std::expected<TOKEN, PARSE_ERRMSG> tokenize(char32_t leading);

private:
  std::vector<mpz_class> bigint_vec;

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
  std::unordered_map<std::string_view, std::size_t> atom_umap;
  std::deque<std::string> atom_vec{};

  std::size_t atom_find(std::string needle);

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

class Parser : public Tokenizer {
public:
  std::vector<STATEMENT> program;
  expected_task<void, PARSE_ERRMSG> parse();

private:
  TOKEN my_token;
  EXPRESSION my_expression;
  STATEMENT my_statement;

  std::expected<void, PARSE_ERRMSG> tokenize();
  expected_task<void, PARSE_ERRMSG> expect_statement_end();
  std::expected<void, PARSE_ERRMSG> expect_punct(char32_t punct);

  expected_task<void, PARSE_ERRMSG> parse_assign_expr();
  expected_task<void, PARSE_ERRMSG> parse_equality_expr();
  expected_task<void, PARSE_ERRMSG> parse_relation_expr();
  expected_task<void, PARSE_ERRMSG> parse_additive_expr();
  expected_task<void, PARSE_ERRMSG> parse_postfix_expr();
  expected_task<void, PARSE_ERRMSG> parse_primary_expr();
  expected_task<void, PARSE_ERRMSG> parse_call_expr();
  expected_task<void, PARSE_ERRMSG> parse_member_expr();
  expected_task<void, PARSE_ERRMSG> parse_access_expr();
  expected_task<void, PARSE_ERRMSG> parse_object_literal();
  expected_task<void, PARSE_ERRMSG> parse_logical_conjunct();
  expected_task<void, PARSE_ERRMSG> parse_logical_disjunct();
  expected_task<void, PARSE_ERRMSG> parse_paren_expr();

  expected_task<void, PARSE_ERRMSG> parse_import();
  expected_task<void, PARSE_ERRMSG> parse_variable_decl();
  expected_task<void, PARSE_ERRMSG> parse_function_decl(EXPRESSION identifier);
  expected_task<void, PARSE_ERRMSG> parse_stmt_expression();
  expected_task<void, PARSE_ERRMSG> parse_ident_statement();
  expected_task<void, PARSE_ERRMSG> parse_punct_statement();
  std::expected<void, PARSE_ERRMSG> parse_statement();
};
} // namespace Language
} // namespace Manadrain
