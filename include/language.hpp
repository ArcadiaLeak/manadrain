#include <cstdint>
#include <generator>
#include <map>
#include <optional>
#include <ranges>
#include <stack>
#include <variant>
#include <vector>

namespace Manadrain {
class Language {
public:
  struct IDENTIFIER {
    std::size_t pool_idx;
    bool is_wellknown;
  };
  struct STRING_LITERAL {
    std::size_t pool_idx;
    char32_t separator;
    bool is_wellknown;
  };
  using TOKEN = std::variant<std::monostate, char32_t, double, std::int64_t,
                             IDENTIFIER, STRING_LITERAL>;

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
  class EXCEPTION : public std::exception {
  public:
    using MESSAGE = std::variant<
        INVALID_NUMBER_LITERAL, INVALID_PROPERTY_NAME, INVALID_BACKSLASH_ESCAPE,
        INVALID_TYPE_ANNOTATION, MISSING_FIELD_NAME, MISSING_VARIABLE_NAME,
        MISSING_FUNCTION_NAME, MISSING_IDENTIFIER, MISSING_STRING_LITERAL,
        MISSING_FORMAL_PARAMETER, MISSING_TYPE_ANNOTATION, MISSING_PUNCT,
        UNEXPECTED_RESERVED_WORD, UNEXPECTED_STRING_END, UNEXPECTED_COMMENT_END,
        UNEXPECTED_TOKEN>;
    explicit EXCEPTION(MESSAGE msg) : message{msg} {}
    const char *what() const noexcept override { return "language exception!"; }

  private:
    MESSAGE message;
  };

  std::vector<std::uint8_t> text_input;
  std::size_t position;
  std::stack<int> backtrace;

  void prev();
  std::int32_t next();
  void backtrack(std::size_t N);

  IDENTIFIER tokenize_identifier(std::int32_t leading);
  STRING_LITERAL tokenize_string_literal(char32_t separator);

  TOKEN tokenize();

private:
  void skip_lf();
  std::optional<char32_t> decode_string_escape(std::int32_t ahead);
  struct DEDUP_RET {
    std::size_t pool_idx;
    bool is_wellknown;
  };
  DEDUP_RET deduplicate_string(std::string potential_copy);
  std::generator<char32_t> traverse();

  std::map<std::string, std::size_t> string_dedup;
  std::vector<std::string_view> string_pool;
};
} // namespace Manadrain
