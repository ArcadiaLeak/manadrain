#include <cstdint>
#include <generator>
#include <inplace_vector>
#include <memory>
#include <ranges>
#include <vector>

namespace Manadrain {
struct FunctionType {
  std::inplace_vector<std::uint8_t, 64> arguments;
  std::optional<std::uint8_t> return_type;
};

struct FunctionImport {
  std::string module_name;
  std::string field_name;
  std::uint32_t type_index;
};

class Parser {
public:
  std::unique_ptr<const std::vector<std::uint8_t>> binary_buffer;
  void parse_binary();

private:
  std::size_t position;

  std::generator<std::uint8_t> traverse();
  std::uint8_t forward();

  std::uint32_t take_vars32();
  std::uint32_t take_varu32();

  std::vector<std::unique_ptr<const FunctionType>> function_types;
  std::vector<std::unique_ptr<const FunctionImport>> function_imports;
  std::vector<std::uint32_t> function_declarations;

  void parse_function_type();
  void parse_import_entry();
};
} // namespace Manadrain
