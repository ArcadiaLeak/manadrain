#include <unicode/uchar.h>
#include <unicode/ustring.h>

#include <format>
#include <memory>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>

#include "manadrain.hpp"

namespace Manadrain {
std::optional<char32_t> ParseBuffer::read(std::string::size_type& idx) {
  UChar32 ch;
  U8_NEXT(buffer.data(), idx, buffer.size(), ch);
  if (ch == U_SENTINEL)
    return std::nullopt;
  return ch;
}
}  // namespace Manadrain
