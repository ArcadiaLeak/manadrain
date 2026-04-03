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

bool ParseDriver::parseHex(std::uint32_t& digit) {
  std::optional uchar{shift()};
  if (not uchar)
    return 0;
  if (*uchar >= '0' && *uchar <= '9') {
    digit = *uchar - '0';
    return 1;
  }
  if (*uchar >= 'A' && *uchar <= 'F') {
    digit = *uchar - 'A' + 10;
    return 1;
  }
  if (*uchar >= 'a' && *uchar <= 'f') {
    digit = *uchar - 'a' + 10;
    return 1;
  }
  return 0;
}

bool ParseDriver::parseHex_b(std::uint32_t& digit) {
  const ParseState state_backup{state};
  bool ok = parseHex(digit);
  if (not ok)
    state = state_backup;
  return ok;
}

bool ParseDriver::parseEscape(ESC_RULE esc_rule,
                              std::pair<char32_t, BAD_ESCAPE>& either) {
  std::optional head{shift()};
  if (not head) {
    either.second = BAD_ESCAPE::PER_SE_BACKSLASH;
    return 0;
  }
  switch (*head) {
    case 'b':
      either.first = '\b';
      return 1;
    case 'f':
      either.first = '\f';
      return 1;
    case 'n':
      either.first = '\n';
      return 1;
    case 'r':
      either.first = '\r';
      return 1;
    case 't':
      either.first = '\t';
      return 1;
    case 'v':
      either.first = '\v';
      return 1;
    default:
      either.second = BAD_ESCAPE::PER_SE_BACKSLASH;
      return 0;
  }
}

bool ParseDriver::parseEscape_b(ESC_RULE esc_rule,
                                std::pair<char32_t, BAD_ESCAPE>& either) {
  const ParseState state_backup{state};
  bool ok = parseEscape(esc_rule, either);
  if (not ok)
    state = state_backup;
  return ok;
}
}  // namespace Manadrain
