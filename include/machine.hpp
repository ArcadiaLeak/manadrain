#include <cstdint>
#include <generator>
#include <memory>
#include <ranges>
#include <vector>

namespace Manadrain {
class Parser {
public:
  std::unique_ptr<const std::vector<std::uint8_t>> binary_buffer;
  void parse_binary();

private:
  std::size_t position;
  std::generator<std::uint8_t> traverse();
  std::optional<std::uint8_t> forward();
  std::uint32_t decode_varuint32();

  void parse_function_type();
};
} // namespace Manadrain
