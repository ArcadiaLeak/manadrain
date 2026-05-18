#include <cstdint>
#include <flat_map>
#include <generator>
#include <list>
#include <memory>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Manadrain {
enum class RESERVED {
  MONOSTATE,
  W_CONST,
  W_LET,
  W_VAR,
  W_CLASS,
  W_FUNCTION,
  W_RETURN,
  W_IMPORT,
  W_EXPORT,
  W_FROM,
  W_AS,
  W_DEFAULT,
  W_UNDEFINED,
  W_NULL,
  W_TRUE,
  W_FALSE,
  W_IF,
  W_ELSE,
  W_WHILE,
  W_FOR,
  W_DO,
  W_BREAK,
  W_CONTINUE,
  W_SWITCH
};
struct IDENTIFIER {
  std::size_t pool_idx;
};
struct STRING_HANDLE {
  std::size_t pool_idx;
};
using NUMERIC_LITERAL = std::variant<std::int64_t, double>;
enum class OPERATOR {
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
using TOKEN = std::variant<std::monostate, char32_t, OPERATOR, NUMERIC_LITERAL,
                           RESERVED, IDENTIFIER, STRING_HANDLE>;

struct INVALID_NUMERIC_LITERAL {};
struct INVALID_PROPERTY_NAME {};
struct INVALID_BACKSLASH_ESCAPE {};
struct INVALID_DECLARATION {};
struct MISSING_FIELD_NAME {};
struct MISSING_VARIABLE_NAME {};
struct MISSING_FUNCTION_NAME {};
struct MISSING_IDENTIFIER {};
struct MISSING_STRING_LITERAL {};
struct MISSING_FORMAL_PARAMETER {};
struct MISSING_PUNCT {
  char32_t must_be;
};
struct UNEXPECTED_RESERVED_WORD {};
struct UNEXPECTED_STRING_END {};
struct UNEXPECTED_COMMENT_END {};
struct UNEXPECTED_TOKEN {};
class ScriptError : public std::exception {
public:
  using MESSAGE = std::variant<
      INVALID_NUMERIC_LITERAL, INVALID_PROPERTY_NAME, INVALID_BACKSLASH_ESCAPE,
      INVALID_DECLARATION, MISSING_FIELD_NAME, MISSING_VARIABLE_NAME,
      MISSING_FUNCTION_NAME, MISSING_IDENTIFIER, MISSING_STRING_LITERAL,
      MISSING_FORMAL_PARAMETER, MISSING_PUNCT, UNEXPECTED_RESERVED_WORD,
      UNEXPECTED_STRING_END, UNEXPECTED_COMMENT_END, UNEXPECTED_TOKEN>;
  MESSAGE message;

  explicit ScriptError(MESSAGE msg) : message{msg} {}
  const char *what() const noexcept override { return "script error!"; }
};

struct EXPR_HANDLE {
  std::size_t pool_idx;
};
using EXPRESSION = std::variant<std::monostate, STRING_HANDLE, NUMERIC_LITERAL,
                                IDENTIFIER, EXPR_HANDLE>;

struct EXPR_BINARY {
  EXPRESSION left;
  EXPRESSION right;
  char32_t op;
};
struct EXPR_MEMBER {
  EXPRESSION object;
  IDENTIFIER property;
};
struct EXPR_CALL {
  EXPRESSION callee;
  std::vector<EXPRESSION> param_vec;
};
using EXPR_NODE = std::variant<EXPR_BINARY, EXPR_MEMBER, EXPR_CALL>;

struct VARIABLE_DECL {
  std::size_t variable_name;
  EXPRESSION initializer;
};
struct STMT_RETURN {
  EXPRESSION return_expr;
};
struct FUNCTION_HANDLE {
  std::size_t pool_idx;
};
using STATEMENT =
    std::variant<EXPRESSION, VARIABLE_DECL, STMT_RETURN, FUNCTION_HANDLE>;

using DYNAMIC = std::optional<
    std::variant<std::monostate, STRING_HANDLE, std::optional<std::size_t>>>;

struct FUNCTION_DECL {
  std::size_t function_name;
  std::flat_map<std::size_t, DYNAMIC> function_scope;
  std::vector<STATEMENT> function_body;
};

class TokenData {
public:
  void text_set(std::vector<std::uint8_t> text) {
    text_source = std::move(text);
  }

  std::generator<std::optional<char32_t>> traverse_text();
  std::optional<char32_t> forward();
  void backward();
  void backward(std::size_t N);

  std::optional<TOKEN> revoked_pull();
  void history_pull();
  void history_push(TOKEN token);

private:
  std::vector<std::uint8_t> text_source;
  std::size_t position;
  std::vector<std::int32_t> backtrace;

  std::list<TOKEN> token_history;
  std::list<TOKEN> token_revoked;
};

class Script {
public:
  void parse_text(std::vector<std::uint8_t> source);
  void execute();

private:
  std::unordered_map<std::string, std::size_t> atom_atlas;
  std::vector<std::string> atom_pool;

  std::vector<EXPR_NODE> expr_pool;
  std::vector<FUNCTION_DECL> func_pool;
  std::flat_map<std::size_t, DYNAMIC> script_scope;
  std::vector<STATEMENT> script_body;

  std::optional<std::indirect<TokenData>> token_data;
  TokenData &tokenization() { return *token_data.value(); }

  TOKEN tokenize_word(char32_t leading);
  TOKEN tokenize_string_literal(char32_t separator);
  TOKEN tokenize_numeric_literal(char32_t leading);
  std::generator<TOKEN> traverse_tokens();
  TOKEN tokenize();

  EXPRESSION parse_primary_expr();
  EXPRESSION parse_postfix_expr();
  EXPRESSION parse_additive_expr();
  EXPRESSION parse_member_expr(EXPRESSION obj_expr);
  EXPRESSION parse_call_expr(EXPRESSION callee_expr);
  EXPRESSION parse_expression();

  void parse_statement(std::flat_map<std::size_t, DYNAMIC> &block_scope,
                       std::vector<STATEMENT> &body, TOKEN leading);
  FUNCTION_DECL parse_function_decl();
  VARIABLE_DECL parse_variable_decl();
};
} // namespace Manadrain
