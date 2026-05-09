#include <gmpxx.h>

#include <deque>
#include <expected>
#include <generator>
#include <optional>
#include <ranges>
#include <span>
#include <stack>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "expected_task.hpp"
#include "machine.hpp"

namespace Manadrain {
struct INVALID_NUMBER_LITERAL {};
struct INVALID_BIGINT_LITERAL {};
struct INVALID_PROPERTY_NAME {};
struct INVALID_BACKSLASH_ESCAPE {};
struct INVALID_TYPE_ANNOTATION {};
struct MISSING_FIELD_NAME {};
struct MISSING_VARIABLE_NAME {};
struct MISSING_FUNCTION_NAME {};
struct MISSING_IDENTIFIER {};
struct MISSING_FROM_CLAUSE {};
struct MISSING_STRING_LITERAL {};
struct MISSING_FORMAL_PARAMETER {};
struct MISSING_TYPE_ANNOTATION {};
struct UNEXPECTED_PUNCT {
  char32_t must_be;
};
struct UNEXPECTED_RESERVED_WORD {};
struct UNEXPECTED_STRING_END {};
struct UNEXPECTED_COMMENT_END {};
struct UNEXPECTED_TOKEN {};
using PARSE_ERR = std::variant<
    INVALID_NUMBER_LITERAL, INVALID_BIGINT_LITERAL, INVALID_PROPERTY_NAME,
    INVALID_BACKSLASH_ESCAPE, INVALID_TYPE_ANNOTATION, MISSING_FIELD_NAME,
    MISSING_VARIABLE_NAME, MISSING_FUNCTION_NAME, MISSING_IDENTIFIER,
    MISSING_FROM_CLAUSE, MISSING_STRING_LITERAL, MISSING_FORMAL_PARAMETER,
    MISSING_TYPE_ANNOTATION, UNEXPECTED_PUNCT, UNEXPECTED_RESERVED_WORD,
    UNEXPECTED_STRING_END, UNEXPECTED_COMMENT_END, UNEXPECTED_TOKEN>;

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
  TOKV_FLOAT,
  TOKV_OP,
  TOKV_BIGINT,
  TOKV_INT
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
                           double, TOK_OPERATOR, TOK_BIGINT, std::int64_t>;

enum EXPRV_INDEX {
  EXPRV_NIL,
  EXPRV_STRING,
  EXPRV_IDENTI,
  EXPRV_NUMBER,
  EXPRV_PTR
};
struct EXPR_NUMBER {
  std::variant<double, std::int64_t> alt;
};
struct EXPR_PTR {
  std::ptrdiff_t expr_idx;
};
using EXPRESSION =
    std::variant<std::monostate, TOK_STRING, TOK_IDENTI, EXPR_NUMBER, EXPR_PTR>;

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
struct EXPR_OBJECT {
  std::vector<std::pair<EXPRESSION, EXPRESSION>> props;
};
struct EXPR_ACCESS {
  EXPRESSION object;
  EXPRESSION property;
};
struct EXPR_ASSIGN {
  EXPRESSION left;
  EXPRESSION right;
};
struct EXPR_LOGIC {
  EXPRESSION left;
  EXPRESSION right;
  TOKEN op;
};
using EXPR_NODE = std::variant<EXPR_CALL, EXPR_MEMBER, EXPR_BINARY, EXPR_OBJECT,
                               EXPR_ACCESS, EXPR_ASSIGN, EXPR_LOGIC>;

struct DECL_VARIABLE {
  std::size_t kind;
  TOK_IDENTI identifier;
  EXPRESSION initializer;
};
struct DECL_IMPORT {
  std::size_t specifiers;
  TOK_STRING source;
};
struct STMT_RETURN {
  EXPRESSION argument;
};
struct STMT_PTR {
  std::ptrdiff_t stmt_idx;
};
using STATEMENT = std::variant<std::monostate, DECL_VARIABLE, EXPRESSION,
                               STMT_PTR, STMT_RETURN, DECL_IMPORT>;

struct STMT_BLOCK {
  std::vector<STATEMENT> subprogram;
};
struct STMT_IF {
  EXPRESSION condition;
  STATEMENT consequent;
  STATEMENT alternate;
};
struct DECL_FUNCTION {
  EXPRESSION identifier;
  MACHINE_DATATYPE return_type;
  std::vector<std::size_t> arguments;
  std::vector<STATEMENT> subprogram;
};
using STMT_NODE = std::variant<STMT_BLOCK, STMT_IF, DECL_FUNCTION>;

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
  std::expected<TOKEN, PARSE_ERR> tokenize(char32_t leading);

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
  using NUM_REPRESENT = std::variant<WHOLE, FRACTIONAL, SCIENTIFIC>;

  std::optional<TOK_0PREFIX> decode_0prefix();
  std::string scan_numseq(std::optional<TOK_0PREFIX> base_opt,
                          std::optional<char32_t> ahead);
};

class TokAtom : public TokNumber {
private:
  std::size_t atom_find(std::string needle);

  std::optional<char32_t> decode_string_esc8();
  std::optional<char32_t> decode_string_xseq();
  std::optional<char32_t> decode_string_uni();

  std::generator<std::expected<char32_t, PARSE_ERR>>
  traverse_string(char32_t separator);
  std::generator<std::optional<char32_t>> traverse_identif(bool &has_escape);

protected:
  std::unordered_map<std::string_view, std::size_t> atom_umap;
  std::deque<std::string> atom_deq{};

  std::expected<TOKEN, PARSE_ERR> tokenize_string(char32_t separator);
  std::expected<TOKEN, PARSE_ERR> tokenize_identif(char32_t leading);
  std::optional<char32_t> decode_identif_uni();
};

class Tokenizer : public TokAtom {
protected:
  std::expected<TOKEN, PARSE_ERR> tokenize();
  bool newlineSeen() { return newline_seen; }

private:
  bool newline_seen;
};

class Parser : public Tokenizer {
public:
  std::vector<STATEMENT> program;
  expected_task<void, PARSE_ERR> parse();

protected:
  std::vector<STMT_NODE> stmt_vec;
  std::vector<EXPR_NODE> expr_vec;
  std::vector<std::vector<TOK_IDENTI>> specifier_vec;

private:
  TOKEN my_token;

  std::expected<void, PARSE_ERR> tokenize();
  expected_task<void, PARSE_ERR> expect_statement_end();
  std::expected<void, PARSE_ERR> expect_punct(char32_t punct);

  expected_task<EXPRESSION, PARSE_ERR> parse_assign_expr();
  expected_task<EXPRESSION, PARSE_ERR> parse_equality_expr();
  expected_task<EXPRESSION, PARSE_ERR> parse_relation_expr();
  expected_task<EXPRESSION, PARSE_ERR> parse_additive_expr();
  expected_task<EXPRESSION, PARSE_ERR> parse_postfix_expr();
  std::expected<EXPRESSION, PARSE_ERR> parse_primary_expr();
  expected_task<EXPRESSION, PARSE_ERR> parse_call_expr(EXPRESSION callee_expr);
  expected_task<EXPRESSION, PARSE_ERR> parse_member_expr(EXPRESSION obj_expr);
  expected_task<EXPRESSION, PARSE_ERR> parse_access_expr(EXPRESSION obj_expr);
  expected_task<EXPRESSION, PARSE_ERR> parse_object_literal();
  expected_task<EXPRESSION, PARSE_ERR> parse_logical_conjunct();
  expected_task<EXPRESSION, PARSE_ERR> parse_logical_disjunct();
  expected_task<EXPRESSION, PARSE_ERR> parse_paren_expr();

  expected_task<STATEMENT, PARSE_ERR> parse_import();
  expected_task<STATEMENT, PARSE_ERR> parse_variable_decl();
  expected_task<STATEMENT, PARSE_ERR>
  parse_function_decl(EXPRESSION identifier);
  expected_task<STATEMENT, PARSE_ERR> parse_stmt_expression();
  expected_task<STATEMENT, PARSE_ERR> parse_ident_statement();
  expected_task<STATEMENT, PARSE_ERR> parse_punct_statement();
  std::expected<STATEMENT, PARSE_ERR> parse_statement();
};

struct FUNCTION_IR {
  MACHINE_DATATYPE return_type;
  std::vector<MACHINE_CMD> command_vec;
};

enum class COMPILE_ERR { UNSUPPORTED, RESERVED_WORD, TYPE_MISMATCH };
class Language : public Parser {
private:
  std::stack<FUNCTION_IR> scope_stack;
  std::inplace_vector<MACHINE_DATATYPE, 32> regfile_type;

public:
  Machine machine;
  expected_task<void, COMPILE_ERR> compile();
  std::expected<MACHINE_CMD, COMPILE_ERR>
  append_cast(bool is_implicit, MACHINE_DATATYPE from, MACHINE_DATATYPE to);

  expected_task<void, COMPILE_ERR> operator()(std::int64_t num);
  expected_task<void, COMPILE_ERR> operator()(EXPR_NUMBER expr);

  expected_task<void, COMPILE_ERR> operator()(EXPR_BINARY &expr);
  expected_task<void, COMPILE_ERR> operator()(EXPR_PTR expr_ptr);

  expected_task<void, COMPILE_ERR> operator()(STMT_RETURN ret_stmt);

  expected_task<void, COMPILE_ERR> operator()(DECL_FUNCTION &decl);
  expected_task<void, COMPILE_ERR> operator()(STMT_PTR stmt_ptr);

  template <typename T> expected_task<void, COMPILE_ERR> operator()(T &stmt) {
    co_return std::unexpected{COMPILE_ERR::UNSUPPORTED};
  }
};
} // namespace Manadrain
