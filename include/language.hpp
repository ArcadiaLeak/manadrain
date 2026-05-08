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
#include "machine.hpp"

namespace Manadrain {
enum class INVALID_ERR {
  NUMBER_LITERAL,
  BIGINT_LITERAL,
  PROPERTY_NAME,
  BACKSLASH_ESCAPE,
  RETURN_TYPE
};
enum class UNEXPECT_ERR { STRING_END, COMMENT_END, THIS_TOKEN };
enum class REQUIRED_ERR {
  FIELD_NAME,
  VARIABLE_NAME,
  FUNCTION_NAME,
  IDENTIFIER,
  FROM_CLAUSE,
  STRING_LITERAL,
  FORMAL_PARAMETER,
  RETURN_TYPE
};
struct KEYWORD_ERR {
  std::size_t atom_sh;
};
struct PUNCT_ERR {
  char32_t must_be;
};
using PARSE_ERR = std::variant<INVALID_ERR, UNEXPECT_ERR, REQUIRED_ERR,
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
  TOKV_FLOAT,
  TOKV_OP,
  TOKV_BIGINT,
  TOKV_INT
};
struct DOUBLE_EQUALS {
  bool operator==(const DOUBLE_EQUALS &) const = default;
};
struct TRIPLE_EQUALS {
  bool operator==(const TRIPLE_EQUALS &) const = default;
};
struct BANG_EQUALS {
  bool operator==(const BANG_EQUALS &) const = default;
};
struct BANG_DOUBLE_EQUALS {
  bool operator==(const BANG_DOUBLE_EQUALS &) const = default;
};
struct LOGICAL_CONJUNCT {
  bool operator==(const LOGICAL_CONJUNCT &) const = default;
};
struct LOGICAL_DISJUNCT {
  bool operator==(const LOGICAL_DISJUNCT &) const = default;
};
struct OP_ADDITION {
  bool operator==(const OP_ADDITION &) const = default;
};
struct OP_SUBTRACT {
  bool operator==(const OP_SUBTRACT &) const = default;
};
using TOK_OPERATOR =
    std::variant<DOUBLE_EQUALS, TRIPLE_EQUALS, BANG_EQUALS, BANG_DOUBLE_EQUALS,
                 LOGICAL_CONJUNCT, LOGICAL_DISJUNCT, OP_ADDITION, OP_SUBTRACT>;
enum class TOK_ASSIGN {
  DIVIDE,
  BITWISE_CONJUNCT,
  LOGICAL_CONJUNCT,
  BITWISE_DISJUNCT,
  LOGICAL_DISJUNCT
};
using TOKEN =
    std::variant<std::monostate, char32_t, TOK_STRING, TOK_IDENTI, double,
                 TOK_OPERATOR, TOK_BIGINT, std::int64_t, TOK_ASSIGN>;

struct EXPR_CALL;
struct EXPR_MEMBER;
struct EXPR_BINARY;
struct EXPR_OBJECT;
struct EXPR_ACCESS;
struct EXPR_ASSIGN;
struct EXPR_LOGIC;
using EXPR_NODE = std::variant<EXPR_CALL, EXPR_MEMBER, EXPR_BINARY, EXPR_OBJECT,
                               EXPR_ACCESS, EXPR_ASSIGN, EXPR_LOGIC>;

enum EXPRV_INDEX {
  EXPRV_NIL,
  EXPRV_STRING,
  EXPRV_IDENTI,
  EXPRV_NUMBER,
  EXPRV_PTR
};
using EXPR_NUMBER = std::variant<double, std::int64_t>;
using EXPRESSION = std::variant<std::monostate, TOK_STRING, TOK_IDENTI,
                                EXPR_NUMBER, std::unique_ptr<EXPR_NODE>>;
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
  TOK_OPERATOR op;
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

enum class COMPILE_ERR { UNSUPPORTED, RESERVED_WORD, TYPE_MISMATCH };
struct DECL_FUNCTION;
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
  MACHINE_DATATYPE return_type;
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

private:
  TOKEN my_token;
  EXPRESSION my_expression;
  STATEMENT my_statement;

  std::expected<void, PARSE_ERR> tokenize();
  expected_task<void, PARSE_ERR> expect_statement_end();
  std::expected<void, PARSE_ERR> expect_punct(char32_t punct);

  expected_task<void, PARSE_ERR> parse_assign_expr();
  expected_task<void, PARSE_ERR> parse_equality_expr();
  expected_task<void, PARSE_ERR> parse_relation_expr();
  expected_task<void, PARSE_ERR> parse_additive_expr();
  expected_task<void, PARSE_ERR> parse_postfix_expr();
  expected_task<void, PARSE_ERR> parse_primary_expr();
  expected_task<void, PARSE_ERR> parse_call_expr();
  expected_task<void, PARSE_ERR> parse_member_expr();
  expected_task<void, PARSE_ERR> parse_access_expr();
  expected_task<void, PARSE_ERR> parse_object_literal();
  expected_task<void, PARSE_ERR> parse_logical_conjunct();
  expected_task<void, PARSE_ERR> parse_logical_disjunct();
  expected_task<void, PARSE_ERR> parse_paren_expr();

  expected_task<void, PARSE_ERR> parse_import();
  expected_task<void, PARSE_ERR> parse_variable_decl();
  expected_task<void, PARSE_ERR> parse_function_decl(EXPRESSION identifier);
  expected_task<void, PARSE_ERR> parse_stmt_expression();
  expected_task<void, PARSE_ERR> parse_ident_statement();
  expected_task<void, PARSE_ERR> parse_punct_statement();
  std::expected<void, PARSE_ERR> parse_statement();
};

struct FUNCTION_IR {
  MACHINE_DATATYPE return_type;
  std::vector<MACHINE_CMD> command_vec;
};

class Language : public Parser {
private:
  std::stack<FUNCTION_IR> scope_stack;
  std::array<MACHINE_DATATYPE, 256> regfile_type;
  std::uint8_t regfile_idx;

public:
  Machine machine;
  expected_task<void, COMPILE_ERR> compile();

  struct MAKE_CONV {};
  struct MAKE_BINARY {};
  using DISPATCH_TAG = std::variant<MAKE_CONV, MAKE_BINARY>;

  std::expected<MACHINE_CMD, COMPILE_ERR>
  operator()(MAKE_CONV, DATATYPE_U64 lhs, DATATYPE_I32 rhs);
  std::expected<MACHINE_CMD, COMPILE_ERR>
  operator()(MAKE_CONV, DATATYPE_I64 lhs, DATATYPE_I32 rhs);
  template <typename T, typename U>
  std::expected<MACHINE_CMD, COMPILE_ERR> operator()(MAKE_CONV, T lhs, U rhs) {
    return std::unexpected{COMPILE_ERR::TYPE_MISMATCH};
  }

  // std::expected<MACHINE_CMD, COMPILE_ERR>
  // operator()(MAKE_BINARY, std::int64_t lhs, std::int64_t rhs);
  // std::expected<MACHINE_CMD, COMPILE_ERR>
  // operator()(MAKE_BINARY, EXPR_NUMBER &lhs, EXPR_NUMBER &rhs);
  template <typename T, typename U>
  std::expected<MACHINE_CMD, COMPILE_ERR> operator()(MAKE_BINARY, T lhs,
                                                     const U &rhs) {
    return std::unexpected{COMPILE_ERR::TYPE_MISMATCH};
  }

  expected_task<void, COMPILE_ERR> operator()(std::int64_t num);
  expected_task<void, COMPILE_ERR> operator()(EXPR_NUMBER &expr);

  expected_task<void, COMPILE_ERR> operator()(std::unique_ptr<EXPR_NODE> &expr);
  expected_task<void, COMPILE_ERR> operator()(EXPR_BINARY &expr);

  expected_task<void, COMPILE_ERR> operator()(DECL_FUNCTION &decl);
  expected_task<void, COMPILE_ERR> operator()(STMT_RETURN &ret_stmt);

  template <typename T> expected_task<void, COMPILE_ERR> operator()(T &stmt) {
    co_return std::unexpected{COMPILE_ERR::UNSUPPORTED};
  }
};
} // namespace Manadrain
