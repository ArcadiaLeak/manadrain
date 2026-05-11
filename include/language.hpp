#include <cstdint>
#include <generator>
#include <map>
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
  std::generator<char32_t> traverse();

  std::map<std::string, std::size_t> string_dedup;
  std::vector<std::string_view> string_pool;
};
} // namespace Manadrain
