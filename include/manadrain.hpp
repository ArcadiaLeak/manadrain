#include <string>

namespace Manadrain {
struct ParseState {
  std::int32_t idx;
};

struct ParseDriver {
  std::basic_string<std::uint8_t> buffer;
  ParseState state;

  std::optional<char32_t> peek();
  std::optional<char32_t> shift();
  void drop(std::int32_t count);
};
}  // namespace Manadrain
