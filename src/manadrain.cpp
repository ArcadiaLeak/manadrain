#include <unicode/uchar.h>
#include <unicode/ustring.h>

#include <format>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Manadrain {
bool is_hi_surrogate(char32_t c) {
  return (c >> 10) == (0xD800 >> 10); /* 0xD800-0xDBFF */
}
bool is_lo_surrogate(char32_t c) {
  return (c >> 10) == (0xDC00 >> 10); /* 0xDC00-0xDFFF */
}
char32_t from_surrogate(char32_t hi, char32_t lo) {
  return 0x10000 + 0x400 * (hi - 0xD800) + (lo - 0xDC00);
}

template <std::ranges::range T>
std::optional<std::ranges::range_value_t<T>> view_peek(T r) {
  if (r.empty())
    return std::nullopt;
  return r.front();
}

template <std::ranges::range T>
std::ranges::range_value_t<T> view_shift(T& r) {
  std::ranges::range_value_t<T> first = r.front();
  r = T{std::ranges::subrange{std::next(r.begin()), r.end()}};
  return first;
}

template <std::ranges::range T>
std::optional<std::ranges::range_value_t<T>> view_shift_opt(T& r) {
  if (r.empty())
    return std::nullopt;
  return view_shift(r);
}

template <std::ranges::range T>
bool view_has_prefix(T lhs, T rhs) {
  return std::ranges::equal(lhs | std::views::take(rhs.size()), rhs);
}

template <std::ranges::range T>
void view_drop(T& r, std::ranges::range_difference_t<T> count) {
  r = r | std::views::drop(count);
}

template <std::ranges::range T>
bool view_drop_if(T& r, std::ranges::range_value_t<T> val) {
  if (not view_peek(r) == val)
    return false;
  view_drop(r, 1);
  return true;
}

template <std::ranges::range T>
bool view_drop_if(T& lhs, T rhs) {
  if (not view_has_prefix(lhs, rhs))
    return false;
  view_drop(lhs, rhs.size());
  return true;
}

template <std::ranges::range T,
          std::predicate<std::ranges::range_value_t<T>> Pred>
bool view_drop_if(T& r, Pred pred) {
  if (r.empty() || not pred(r.front()))
    return false;
  view_drop(r, 1);
  return true;
}

struct UnitMetadata {
  std::uint32_t line;
  std::uint32_t colu;
};

enum class STRICTNESS { SLOPPY, STRICT };
enum class ESC_RULE {
  IDENTIFIER,
  REGEXP_ASCII,
  REGEXP_UTF16,
  STRING_IN_SLOPPY_MODE,
  STRING_IN_STRICT_MODE,
  STRING_IN_TEMPLATE
};

enum class BAD_ESCAPE { MALFORMED, PER_SE_BACKSLASH, OCTAL_SEQ };
enum class BAD_STRING {
  UNEXPECTED_END,
  OCTAL_SEQ_IN_ESCAPE,
  MALFORMED_SEQ_IN_ESCAPE,
  MISMATCH
};

struct ParseState {
  std::u32string_view textview;
  bool newline_seen;

  bool parse_space();
  bool parse_hex(std::uint32_t& digit);
};

bool ParseState::parse_space() {
  const std::u32string_view start_view{textview};
  newline_seen = false;

  while (true) {
    if (view_peek(textview) == '\n')
      newline_seen = true;

    if (view_drop_if(textview, u_isWhitespace))
      continue;

    if (view_drop_if<std::u32string_view>(textview, U"//")) {
      view_drop(textview, std::distance(textview.begin(),
                                        std::ranges::find(textview, '\n')));
      continue;
    }

    if (view_drop_if<std::u32string_view>(textview, U"/*")) {
      static constexpr std::u32string_view end_delim{U"*/"};
      while (true) {
        if (textview.empty() || view_has_prefix(textview, end_delim))
          break;
        if (view_shift(textview) == '\n')
          newline_seen = true;
      }
      view_drop(textview, end_delim.size());
      continue;
    }

    break;
  }

  return textview.size() < start_view.size();
}

bool ParseState::parse_hex(std::uint32_t& digit) {
  if (textview.empty())
    return 0;
  char32_t uchar{view_shift(textview)};
  if (uchar >= '0' && uchar <= '9') {
    digit = uchar - '0';
    return 1;
  }
  if (uchar >= 'A' && uchar <= 'F') {
    digit = uchar - 'A' + 10;
    return 1;
  }
  if (uchar >= 'a' && uchar <= 'f') {
    digit = uchar - 'a' + 10;
    return 1;
  }
  return 0;
}

struct ParseData {
  std::vector<UnitMetadata> meta;
  std::u32string text;

  void populate_iter(std::basic_string_view<UChar32>& textview);
  void populate(const std::string& narrow);
};

void ParseData::populate_iter(std::basic_string_view<UChar32>& textview) {
  UChar32 ch = view_shift(textview);

  if (ch == '\r' && view_peek(textview) == '\n')
    view_drop(textview, 1);

  bool newline_seen{};
  if (ch == '\r' || ch == '\n' || ch == 0x2028 || ch == 0x2029) {
    ch = '\n';
    newline_seen = 1;
  }

  text.push_back(ch);
  meta.emplace_back(
      UnitMetadata{.line = meta.back().line + newline_seen,
                   .colu = newline_seen ? 0 : meta.back().colu + 1});
}

void ParseData::populate(const std::string& narrow) {
  UErrorCode status = U_ZERO_ERROR;

  std::int32_t u16_size{};
  u_strFromUTF8(nullptr, 0, &u16_size, narrow.data(), -1, &status);
  if (status == U_BUFFER_OVERFLOW_ERROR)
    status = U_ZERO_ERROR;

  std::u16string medium{};
  medium.resize(u16_size);
  u_strFromUTF8(medium.data(), u16_size + 1, nullptr, narrow.data(), -1,
                &status);

  if (U_FAILURE(status))
    throw std::runtime_error{
        std::format("UTF-8 to UTF-16 failed: {}", u_errorName(status))};

  std::int32_t u32_size{};
  u_strToUTF32(nullptr, 0, &u32_size, medium.data(), -1, &status);
  if (status == U_BUFFER_OVERFLOW_ERROR)
    status = U_ZERO_ERROR;

  std::basic_string<UChar32> wide{};
  wide.resize(u32_size);
  u_strToUTF32(wide.data(), u32_size + 1, nullptr, medium.data(), -1, &status);

  if (U_FAILURE(status))
    throw std::runtime_error{
        std::format("UTF-16 to UTF-32 failed: {}", u_errorName(status))};

  std::basic_string_view<UChar32> textview{wide};
  meta.emplace_back(UnitMetadata{.line = 1, .colu = 0});
  while (not textview.empty())
    populate_iter(textview);
  meta.pop_back();
}

namespace Token {
struct String {
  char32_t sep;
  std::u32string buffer;
};
}  // namespace Token

namespace Parser {
struct Common {
  std::span<UnitMetadata> metadata;
  ParseState state;

  bool parse_hex_b(std::uint32_t& digit);
  bool parse_escape(ESC_RULE esc_rule, std::pair<char32_t, BAD_ESCAPE>& parsed);
  bool parse_escape_b(ESC_RULE esc_rule,
                      std::pair<char32_t, BAD_ESCAPE>& parsed);
  bool parse_escape_hex(std::pair<char32_t, BAD_ESCAPE>& parsed);
  bool parse_escape_uni(ESC_RULE esc_rule,
                        std::pair<char32_t, BAD_ESCAPE>& parsed);
  bool parse_escape_uni_braced(std::pair<char32_t, BAD_ESCAPE>& parsed);
  bool parse_escape_uni_fixed(ESC_RULE esc_rule,
                              std::pair<char32_t, BAD_ESCAPE>& parsed);
};

bool Common::parse_hex_b(std::uint32_t& digit) {
  ParseState fork_state{state};
  bool ok = fork_state.parse_hex(digit);
  if (ok)
    state = fork_state;
  return ok;
}

bool Common::parse_escape_hex(std::pair<char32_t, BAD_ESCAPE>& parsed) {
  std::uint32_t hex0{};
  if (not parse_hex_b(hex0)) {
    parsed.second = BAD_ESCAPE::MALFORMED;
    return 0;
  }
  std::uint32_t hex1{};
  if (not parse_hex_b(hex1)) {
    parsed.second = BAD_ESCAPE::MALFORMED;
    return 0;
  }
  parsed.first = (hex0 << 4) | hex1;
  return 1;
}

bool Common::parse_escape_uni_braced(std::pair<char32_t, BAD_ESCAPE>& parsed) {
  char32_t utf16_char = 0;
  while (true) {
    std::uint32_t hex{};
    if (not parse_hex_b(hex)) {
      std::optional close_or_uchar = view_shift_opt(state.textview);
      if (not close_or_uchar) {
        parsed.second = BAD_ESCAPE::MALFORMED;
        return 0;
      }
      if (close_or_uchar == '}') {
        parsed.first = utf16_char;
        return 1;
      }
    }
    utf16_char = (utf16_char << 4) | hex;
    if (utf16_char > 0x10FFFF) {
      parsed.second = BAD_ESCAPE::MALFORMED;
      return 0;
    }
  }
}

bool Common::parse_escape_uni_fixed(ESC_RULE esc_rule,
                                    std::pair<char32_t, BAD_ESCAPE>& parsed) {
  char32_t high_surr = 0;
  for (int i = 0; i < 4; i++) {
    std::uint32_t hex{};
    if (not parse_hex_b(hex)) {
      parsed.second = BAD_ESCAPE::MALFORMED;
      return 0;
    }
    high_surr = (high_surr << 4) | hex;
  }

  if (is_hi_surrogate(high_surr) && esc_rule == ESC_RULE::REGEXP_UTF16 &&
      view_drop_if<std::u32string_view>(state.textview, U"\\u")) {
    char32_t low_surr = 0;
    for (int i = 0; i < 4; i++) {
      std::uint32_t hex{};
      if (not parse_hex_b(hex)) {
        parsed.first = high_surr;
        return 1;
      }
      low_surr = (low_surr << 4) | hex;
    }
    if (is_lo_surrogate(low_surr)) {
      parsed.first = from_surrogate(high_surr, low_surr);
      return 1;
    }
  }

  parsed.first = high_surr;
  return 1;
}

bool Common::parse_escape_uni(ESC_RULE esc_rule,
                              std::pair<char32_t, BAD_ESCAPE>& parsed) {
  std::optional open_or_uchar = view_shift_opt(state.textview);
  if (not open_or_uchar) {
    parsed.second = BAD_ESCAPE::MALFORMED;
    return 0;
  }
  return open_or_uchar == '{' && esc_rule != ESC_RULE::REGEXP_ASCII
             ? parse_escape_uni_braced(parsed)
             : parse_escape_uni_fixed(esc_rule, parsed);
}

bool Common::parse_escape(ESC_RULE esc_rule,
                          std::pair<char32_t, BAD_ESCAPE>& parsed) {
  std::optional head = view_shift_opt(state.textview);
  if (not head) {
    parsed.second = BAD_ESCAPE::PER_SE_BACKSLASH;
    return 0;
  }
  switch (*head) {
    case 'b':
      parsed.first = U'\b';
      return 1;
    case 'f':
      parsed.first = U'\f';
      return 1;
    case 'n':
      parsed.first = U'\n';
      return 1;
    case 'r':
      parsed.first = U'\r';
      return 1;
    case 't':
      parsed.first = U'\t';
      return 1;
    case 'v':
      parsed.first = U'\v';
      return 1;
    case 'x':
      return parse_escape_hex(parsed);
    case 'u':
      return parse_escape_uni(esc_rule, parsed);
    case '0': {
      std::optional ahead = view_peek(state.textview);
      if (not ahead.transform([](char32_t uch) { return std::isdigit(uch); })
                  .value_or(false)) {
        parsed.first = 0;
        return 1;
      }
    }
      [[fallthrough]];
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
      switch (esc_rule) {
        case ESC_RULE::STRING_IN_STRICT_MODE:
          parsed.second = BAD_ESCAPE::OCTAL_SEQ;
          return 0;

        case ESC_RULE::STRING_IN_TEMPLATE:
        case ESC_RULE::REGEXP_UTF16:
          parsed.second = BAD_ESCAPE::MALFORMED;
          return 0;

        default:
          parsed.first = *head - '0';
          std::optional<char32_t> ahead;
          ahead = view_peek(state.textview).transform([](char32_t ahead_digit) {
            return ahead_digit - '0';
          });
          if (not ahead || *ahead > 7)
            return 1;
          view_drop(state.textview, 1);
          parsed.first = (parsed.first << 3) | *ahead;

          if (parsed.first >= 32)
            return 1;

          ahead = view_peek(state.textview).transform([](char32_t ahead_digit) {
            return ahead_digit - '0';
          });
          if (not ahead || *ahead > 7)
            return 1;
          view_drop(state.textview, 1);
          parsed.first = (parsed.first << 3) | *ahead;
          return 1;
      }
    default:
      parsed.second = BAD_ESCAPE::PER_SE_BACKSLASH;
      return 0;
  }
}

bool Common::parse_escape_b(ESC_RULE esc_rule,
                            std::pair<char32_t, BAD_ESCAPE>& parsed) {
  const ParseState state_backup{state};
  bool ok = parse_escape(esc_rule, parsed);
  if (not ok)
    state = state_backup;
  return ok;
}
}  // namespace Parser

void Parse(const std::string& src_string) {
  std::shared_ptr parse_data = std::make_shared<ParseData>();
  parse_data->populate(src_string);
}
}  // namespace Manadrain
