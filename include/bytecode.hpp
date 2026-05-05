#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "expected_task.hpp"

namespace Manadrain {
namespace Bytecode {
inline constexpr std::uint32_t WASM_BINARY_MAGIC{0x6d736100};
inline constexpr std::uint32_t WASM_BINARY_VERSION{1};
inline constexpr std::uint32_t WASM_BINARY_LAYER_MODULE{0};

enum class PRIM_TYPE { I32T, I64T, F32T, F64T };
enum class EXTERN_KIND { FUNC, TABLE, MEMORY, GLOBAL };

enum class CORRUPT_ERR {
  UNSIGN_FIXED,
  SIGNED_LEB128,
  UNSIGN_LEB128,
  SHORT_SECTN,
  MULTIVAL_RET
};
enum class INVALID_ERR {
  WASM_MAGIC,
  WASM_LAYER,
  WASM_VERSN,
  SECTN_CODE,
  PARAM_TYPE,
  UTF8_STRING,
  EXTERN_KIND
};
enum class UNEXPECT_ERR { TYPE_FORM };
using READER_ERR = std::variant<CORRUPT_ERR, INVALID_ERR, UNEXPECT_ERR>;

struct FUNC_TYPE {
  std::vector<PRIM_TYPE> param_types;
  std::vector<PRIM_TYPE> result_types;
};

class Reader {
public:
  void populate(const std::vector<std::uint8_t> &buffer_ref);
  void populate(std::vector<std::uint8_t> &&buffer_ref);

  std::vector<FUNC_TYPE> func_types;
  std::vector<std::uint32_t> func_headers;

  expected_task<void, READER_ERR> read_module();

private:
  std::size_t position;
  std::vector<std::uint8_t> buffer;

  std::expected<std::uint32_t, READER_ERR> read_u32(int cnt);
  std::expected<std::uint32_t, READER_ERR> unsign_leb128();
  std::expected<std::int32_t, READER_ERR> signed_leb128();

  expected_task<void, READER_ERR> read_sections();
  expected_task<void, READER_ERR> read_type_section();
  expected_task<void, READER_ERR> read_type_form();
  expected_task<void, READER_ERR> read_function_section();
  expected_task<void, READER_ERR> read_export_section();
};
} // namespace Bytecode
} // namespace Manadrain
