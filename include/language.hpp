#include <gmpxx.h>

#include <deque>
#include <expected>
#include <generator>
#include <inplace_vector>
#include <optional>
#include <ranges>
#include <stack>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

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
struct MISSING_PUNCT {
  char32_t must_be;
};
struct UNEXPECTED_RESERVED_WORD {};
struct UNEXPECTED_STRING_END {};
struct UNEXPECTED_COMMENT_END {};
struct UNEXPECTED_TOKEN {};
class PARSE_ERROR : public std::exception {
public:
  using MESSAGE = std::variant<
      INVALID_NUMBER_LITERAL, INVALID_BIGINT_LITERAL, INVALID_PROPERTY_NAME,
      INVALID_BACKSLASH_ESCAPE, INVALID_TYPE_ANNOTATION, MISSING_FIELD_NAME,
      MISSING_VARIABLE_NAME, MISSING_FUNCTION_NAME, MISSING_IDENTIFIER,
      MISSING_FROM_CLAUSE, MISSING_STRING_LITERAL, MISSING_FORMAL_PARAMETER,
      MISSING_TYPE_ANNOTATION, MISSING_PUNCT, UNEXPECTED_RESERVED_WORD,
      UNEXPECTED_STRING_END, UNEXPECTED_COMMENT_END, UNEXPECTED_TOKEN>;
  explicit PARSE_ERROR(MESSAGE msg) : message{msg} {}
  const char *what() const noexcept override { return "parse error!"; }

private:
  MESSAGE message;
};

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

enum class TYPE_ANNOTATION { I32T, I64T, F32T, F64T, U32T, U64T, TYPE_STR };

struct DECL_VARIABLE {
  std::size_t kind;
  TOK_IDENTI identifier;
  TYPE_ANNOTATION datatype;
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
  TYPE_ANNOTATION return_type;
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
  std::expected<TOKEN, PARSE_ERROR::MESSAGE> tokenize(char32_t leading);

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

  std::generator<std::expected<char32_t, PARSE_ERROR::MESSAGE>>
  traverse_string(char32_t separator);
  std::generator<std::optional<char32_t>> traverse_identif(bool &has_escape);

protected:
  std::unordered_map<std::string_view, std::size_t> atom_umap;
  std::deque<std::string> atom_deq{};

  std::expected<TOKEN, PARSE_ERROR::MESSAGE>
  tokenize_string(char32_t separator);
  std::expected<TOKEN, PARSE_ERROR::MESSAGE> tokenize_identif(char32_t leading);
  std::optional<char32_t> decode_identif_uni();
};

class Tokenizer : public TokAtom {
protected:
  std::expected<TOKEN, PARSE_ERROR::MESSAGE> tokenize();
  bool newlineSeen() { return newline_seen; }

private:
  bool newline_seen;
};

class Parser : public Tokenizer {
public:
  std::vector<STATEMENT> program;
  void parse();

protected:
  std::vector<STMT_NODE> stmt_vec;
  std::vector<EXPR_NODE> expr_vec;
  std::vector<std::vector<TOK_IDENTI>> specifier_vec;

private:
  TOKEN my_token;

  void tokenize();
  void expect_statement_end();
  void expect_punct(char32_t punct);

  EXPRESSION parse_assign_expr();
  EXPRESSION parse_equality_expr();
  EXPRESSION parse_relation_expr();
  EXPRESSION parse_additive_expr();
  EXPRESSION parse_postfix_expr();
  EXPRESSION parse_primary_expr();
  EXPRESSION parse_call_expr(EXPRESSION callee_expr);
  EXPRESSION parse_member_expr(EXPRESSION obj_expr);
  EXPRESSION parse_access_expr(EXPRESSION obj_expr);
  EXPRESSION parse_object_literal();
  EXPRESSION parse_logical_conjunct();
  EXPRESSION parse_logical_disjunct();
  EXPRESSION parse_paren_expr();

  TYPE_ANNOTATION parse_type_annotation();

  STATEMENT parse_import();
  STATEMENT parse_variable_decl();
  STATEMENT parse_function_decl(EXPRESSION identifier);
  STATEMENT parse_stmt_expression();
  STATEMENT parse_ident_statement();
  STATEMENT parse_punct_statement();
  STATEMENT parse_statement();
};

class COMPILE_ERROR : public std::exception {
public:
  enum MESSAGE { UNSUPPORTED, RESERVED_WORD, TYPE_MISMATCH, VOID_IDENTIF };
  explicit COMPILE_ERROR(MESSAGE msg) : message{msg} {}
  const char *what() const noexcept override { return "compile error!"; }

private:
  MESSAGE message;
};

struct FUNCTION_IR {
  struct LOCAL_VAR {
    std::size_t datatype;
    std::size_t identifier;
  };
  std::vector<LOCAL_VAR> local_vec;
  std::size_t return_type;
  std::vector<Machine::INSTRUCTION> inst_vec;
  std::vector<std::vector<std::uint64_t>> const_pool;
  std::unordered_map<std::size_t, std::size_t> const_umap;
};

class Language : public Parser {
public:
  enum TYPEID { I32T, I64T, F32T, F64T, U32T, U64T, HEAP_STR, LITERAL_STR };

  void operator()(TOK_STRING token_str);
  void operator()(DECL_VARIABLE declaration);

  void operator()(DECL_FUNCTION &decl);
  void operator()(STMT_PTR stmt_ptr);

  template <typename T> void operator()(T &visitee) {
    throw COMPILE_ERROR{COMPILE_ERROR::UNSUPPORTED};
  }

  Machine machine;
  void compile();

private:
  std::stack<FUNCTION_IR> scope_stack;
  std::inplace_vector<std::size_t, 32> regfile_type;
};
} // namespace Manadrain
