#include <cstdint>
#include <generator>
#include <optional>
#include <ranges>
#include <set>
#include <stack>
#include <variant>
#include <vector>

namespace Manadrain {
enum class RESERVED {
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
  W_SWITCH,
  W_INT,
  W_LONG,
  W_UINT,
  W_ULONG,
  W_FLOAT,
  W_DOUBLE,
  W_STRING
};
struct IDENTIFIER {
  std::string_view pool_view;
};
struct STRING_LITERAL {
  std::string_view pool_view;
  char32_t separator;
};
using TOKEN = std::variant<std::monostate, char32_t, double, std::int64_t,
                           RESERVED, IDENTIFIER, STRING_LITERAL>;

struct INVALID_NUMBER_LITERAL {};
struct INVALID_PROPERTY_NAME {};
struct INVALID_BACKSLASH_ESCAPE {};
struct INVALID_TYPE_ANNOTATION {};
struct MISSING_FIELD_NAME {};
struct MISSING_VARIABLE_NAME {};
struct MISSING_FUNCTION_NAME {};
struct MISSING_IDENTIFIER {};
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
class LanguageError : public std::exception {
public:
  using MESSAGE = std::variant<
      INVALID_NUMBER_LITERAL, INVALID_PROPERTY_NAME, INVALID_BACKSLASH_ESCAPE,
      INVALID_TYPE_ANNOTATION, MISSING_FIELD_NAME, MISSING_VARIABLE_NAME,
      MISSING_FUNCTION_NAME, MISSING_IDENTIFIER, MISSING_STRING_LITERAL,
      MISSING_FORMAL_PARAMETER, MISSING_TYPE_ANNOTATION, MISSING_PUNCT,
      UNEXPECTED_RESERVED_WORD, UNEXPECTED_STRING_END, UNEXPECTED_COMMENT_END,
      UNEXPECTED_TOKEN>;
  MESSAGE message;

  explicit LanguageError(MESSAGE msg) : message{msg} {}
  const char *what() const noexcept override { return "language exception!"; }
};

class Language {
public:
  std::vector<std::uint8_t> text_input;
  std::size_t position;

  std::stack<std::optional<char32_t>> backtrace;
  std::set<std::string> string_pool;

  std::generator<std::optional<char32_t>> traverse();
  std::optional<char32_t> forward();
  void backward();
  void backward(std::size_t N);

  TOKEN tokenize();
  void compile_text();

private:
  TOKEN tokenize_word();
};

class ParseDeclaration {
public:
  explicit ParseDeclaration(Language *l) : lang{l} {}

  void operator()(RESERVED reserved);
  template <typename T> void operator()(T visitee) {
    throw LanguageError{UNEXPECTED_TOKEN{}};
  }

private:
  Language *lang;
};

class ParseFunctionDeclaration {
public:
  explicit ParseFunctionDeclaration(Language *l) : lang{l} {}

  void operator()(IDENTIFIER identifier);
  template <typename T> void operator()(T visitee) {
    throw LanguageError{UNEXPECTED_TOKEN{}};
  }

private:
  Language *lang;
  int stage;
};
} // namespace Manadrain
