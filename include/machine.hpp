#include <cstdint>
#include <deque>
#include <memory>
#include <memory_resource>
#include <print>
#include <stdfloat>

struct Dynamic;

union Primitive {
  std::byte byte_[8];
  std::int16_t ligature[4];
  std::int32_t number[2];
  std::int64_t product;
  std::float32_t fraction[2];
  std::float64_t quotient;
  const char16_t *wide;
  const Dynamic *dynamic;
};

enum class DynamicTag {
  T_BYTE,
  T_LIGATURE,
  T_NUMBER,
  T_PRODUCT,
  T_FRACTION,
  T_QUOTIENT,
  T_STRING
};

struct Dynamic {
  DynamicTag tag;
  Primitive value;
};

struct FunctionFrame {
  virtual ~FunctionFrame() = default;
  FunctionFrame() = default;
  FunctionFrame(const FunctionFrame &) = default;
  FunctionFrame &operator=(const FunctionFrame &) = default;
  FunctionFrame(FunctionFrame &&) = default;
  FunctionFrame &operator=(FunctionFrame &&) = default;

  std::span<Primitive> own_scope;
};

struct Machine {
  Machine();
  std::array<std::byte, 65536> buffer;
  std::pmr::monotonic_buffer_resource resource;
  std::deque<FunctionFrame *> function_frames;
};
