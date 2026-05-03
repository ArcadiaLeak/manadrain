#include <cstdint>
#include <expected>
#include <vector>

namespace Manadrain {
namespace Bytecode {
enum class READER_ERR { BAD_MAGIC_VALUE };

class Reader {
public:
  void populate(const std::vector<std::uint8_t> &buffer_ref);
  void populate(std::vector<std::uint8_t> &&buffer_ref);
  std::expected<void, READER_ERR> read_module();

private:
  int position;
  std::vector<std::uint8_t> buffer;

  std::uint32_t read4();
};
} // namespace Bytecode
} // namespace Manadrain
