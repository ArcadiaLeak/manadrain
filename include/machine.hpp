#include <cstdint>
#include <generator>
#include <inplace_vector>
#include <memory>
#include <ranges>
#include <vector>

namespace Manadrain {
struct FunctionDefinition {
  std::inplace_vector<std::uint8_t, 64> arguments;
  std::optional<std::uint8_t> return_type;
};

class Parser {
public:
  std::unique_ptr<const std::vector<std::uint8_t>> binary_buffer;
  void parse_binary();

private:
  std::size_t position;

  std::generator<std::uint8_t> head_traverse();
  std::generator<std::uint8_t> tail_traverse();

  std::optional<std::uint8_t> forward();

  std::uint32_t decode_varsint32();
  std::uint32_t decode_varuint32();

  std::vector<std::unique_ptr<const FunctionDefinition>> function_definitions;

  void parse_function_type();
};
} // namespace Manadrain
