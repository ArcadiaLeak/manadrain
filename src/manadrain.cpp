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
std::optional<char32_t> ParseDriver::peek() {
  if (state.idx >= buffer.size())
    return std::nullopt;
  UChar32 ch;
  U8_GET(buffer.data(), 0, state.idx, buffer.size(), ch);
  if (ch < 0)
    return std::nullopt;
  return ch;
}

std::optional<char32_t> ParseDriver::shift() {
  if (state.idx >= buffer.size())
    return std::nullopt;
  UChar32 ch;
  U8_NEXT(buffer.data(), state.idx, buffer.size(), ch);
  if (ch < 0)
    return std::nullopt;
  return ch;
}

void ParseDriver::drop(std::uint32_t count) {
  U8_FWD_N(buffer.data(), state.idx, buffer.size(), count);
}

bool ParseDriver::parseEscape(ESC_RULE esc_rule,
                              std::pair<char32_t, BAD_ESCAPE>& either) {
  std::optional head{shift()};
  if (not head) {
    either.second = BAD_ESCAPE::PER_SE_BACKSLASH;
    return 0;
  }
  return 0;
}

bool ParseDriver::parseEscape_b(ESC_RULE esc_rule,
                                std::pair<char32_t, BAD_ESCAPE>& parsed) {
  const ParseState state_backup{state};
  bool ok = parseEscape(esc_rule, parsed);
  if (not ok)
    state = state_backup;
  return ok;
}
}  // namespace Manadrain
