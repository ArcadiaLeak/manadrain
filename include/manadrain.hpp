#include <ranges>
#include <string>

namespace Manadrain {
struct ParseBuffer {
  std::string buffer;
  std::optional<char32_t> read(std::string::size_type& idx);
};
}  // namespace Manadrain
