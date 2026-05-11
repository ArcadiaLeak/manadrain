#include <cstdint>
#include <stack>
#include <variant>
#include <vector>

namespace Manadrain {
struct Language {
  struct IDENTIFIER {
    std::size_t pool_idx;
    bool is_wellknown;
    bool has_escape;
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

  TOKEN tokenize();
};
} // namespace Manadrain
