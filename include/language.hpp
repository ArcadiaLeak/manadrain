#include <cstdint>
#include <functional>
#include <generator>
#include <inplace_vector>
#include <list>
#include <optional>
#include <ranges>
#include <stack>
#include <unordered_set>
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
  std::string_view pool_view;
};
struct STRING_LITERAL {
  std::string_view pool_view;
  char32_t separator;
};
using NUMERIC_LITERAL = std::variant<std::int64_t, double>;
using TOKEN = std::variant<std::monostate, char32_t, NUMERIC_LITERAL, RESERVED,
                           IDENTIFIER, STRING_LITERAL>;

struct INVALID_NUMERIC_LITERAL {};
struct INVALID_PROPERTY_NAME {};
struct INVALID_BACKSLASH_ESCAPE {};
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
struct UNSUPPORTED {};
class ScriptError : public std::exception {
public:
  using MESSAGE = std::variant<
      INVALID_NUMERIC_LITERAL, INVALID_PROPERTY_NAME, INVALID_BACKSLASH_ESCAPE,
      MISSING_FIELD_NAME, MISSING_VARIABLE_NAME, MISSING_FUNCTION_NAME,
      MISSING_IDENTIFIER, MISSING_STRING_LITERAL, MISSING_FORMAL_PARAMETER,
      MISSING_PUNCT, UNEXPECTED_RESERVED_WORD, UNEXPECTED_STRING_END,
      UNEXPECTED_COMMENT_END, UNEXPECTED_TOKEN, UNSUPPORTED>;
  MESSAGE message;

  explicit ScriptError(MESSAGE msg) : message{msg} {}
  const char *what() const noexcept override { return "script error!"; }
};

using EXPRESSION = std::variant<STRING_LITERAL, NUMERIC_LITERAL, IDENTIFIER>;

using DYNAMIC = std::variant<std::monostate>;
using HEAP_SLOT = std::variant<std::monostate>;

struct FUNCTION_DECL;

struct VARIABLE_DECL {
  std::string_view variable_name;
  EXPRESSION initializer;
};

using STATEMENT = std::variant<std::monostate, FUNCTION_DECL, VARIABLE_DECL>;

struct FUNCTION_DECL {
  std::string_view function_name;
  std::vector<STATEMENT> function_body;
};

class Script {
public:
  std::vector<std::uint8_t> text_source;
  std::size_t position;

  std::list<std::string> atom_pool;
  std::unordered_set<std::string_view> atom_atlas;
  std::vector<HEAP_SLOT> heap;

  void parse_text();

private:
  std::stack<std::optional<char32_t>> backtrace;
  std::vector<STATEMENT> script_body;

  std::generator<std::optional<char32_t>> traverse_text();
  std::optional<char32_t> forward();
  void backward();
  void backward(std::size_t N);

  TOKEN tokenize();
  std::generator<TOKEN> traverse_tokens();
  TOKEN tokenize_word(char32_t leading);
  TOKEN tokenize_string_literal(char32_t separator);
  TOKEN tokenize_numeric_literal(char32_t leading);

  STATEMENT parse_statement(RESERVED reserved);
  template <typename T> STATEMENT parse_statement(T visitee) {
    throw ScriptError{UNEXPECTED_TOKEN{}};
  }
  std::generator<STATEMENT> traverse_script_body();
  std::generator<STATEMENT> traverse_function_body();
  STATEMENT parse_function_decl();
  STATEMENT parse_variable_decl();

  EXPRESSION parse_expression(STRING_LITERAL string_literal);
  template <typename T> EXPRESSION parse_expression(T visitee) {
    throw ScriptError{UNEXPECTED_TOKEN{}};
  }
};
} // namespace Manadrain
