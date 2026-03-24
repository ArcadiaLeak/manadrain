module;
#include <unicode/uchar.h>
#include <unicode/ustring.h>
export module manadrain;
import std;

namespace Manadrain {

bool is_hi_surrogate(std::uint32_t c) {
  return (c >> 10) == (0xD800 >> 10); /* 0xD800-0xDBFF */
}
bool is_lo_surrogate(std::uint32_t c) {
  return (c >> 10) == (0xDC00 >> 10); /* 0xDC00-0xDFFF */
}
std::uint32_t from_surrogate(std::uint32_t hi, std::uint32_t lo) {
  return 0x10000 + 0x400 * (hi - 0xD800) + (lo - 0xDC00);
}

using u32_string = std::basic_string<UChar32>;
using u32_string_view = std::basic_string_view<UChar32>;
u32_string operator""_u32(const char* str, std::size_t len) {
  return u32_string{str, str + len};
}

std::optional<UChar32> next_uchar32(u32_string_view& src_view) {
  if (src_view.empty())
    return std::nullopt;
  UChar32 ch = src_view.front();
  src_view = src_view | std::views::drop(1);
  return ch;
}

std::optional<UChar32> next_uchar32(u32_string_view&& src_view) {
  return next_uchar32(src_view);
}

std::optional<UChar32> hex_digit(UChar32 digit) {
  if (digit >= '0' && digit <= '9')
    return digit - '0';
  if (digit >= 'A' && digit <= 'F')
    return digit - 'A' + 10;
  if (digit >= 'a' && digit <= 'f')
    return digit - 'a' + 10;
  return std::nullopt;
}

enum class BAD_ESCAPE { MALFORMED, MISMATCH };
enum class ESC_RULE {
  IDENTIFIER,
  REGEXP_ASCII,
  REGEXP_UTF16,
  STRING_IN_SLOPPY_MODE,
  STRING_IN_STRICT_MODE,
  STRING_IN_TEMPLATE
};

std::expected<UChar32, BAD_ESCAPE> parse_escape(u32_string_view& src_view,
                                                ESC_RULE esc_rule) {
  auto escape_err = [&src_view, passed_view = src_view](BAD_ESCAPE error_tag) {
    src_view = passed_view;
    return std::unexpected{error_tag};
  };
  std::optional head = next_uchar32(src_view);
  if (not head)
    return escape_err(BAD_ESCAPE::MISMATCH);

  switch (*head) {
    case 'b':
      return '\b';
    case 'f':
      return '\f';
    case 'n':
      return '\n';
    case 'r':
      return '\r';
    case 't':
      return '\t';
    case 'v':
      return '\v';

    case 'x': {
      std::optional hex0 = next_uchar32(src_view).and_then(hex_digit);
      if (not hex0)
        return escape_err(BAD_ESCAPE::MALFORMED);
      std::optional hex1 = next_uchar32(src_view).and_then(hex_digit);
      if (not hex1)
        return escape_err(BAD_ESCAPE::MALFORMED);
      return (*hex0 << 4) | *hex1;
    }

    case 'u': {
      std::optional open_or_uchar = next_uchar32(src_view);
      if (not open_or_uchar)
        return escape_err(BAD_ESCAPE::MALFORMED);
      if (open_or_uchar == '{' && esc_rule != ESC_RULE::REGEXP_ASCII) {
        std::uint32_t utf16_char = 0;

        do {
          std::optional close_or_uchar = next_uchar32(src_view);
          if (not close_or_uchar)
            return escape_err(BAD_ESCAPE::MALFORMED);
          if (close_or_uchar == '}')
            return utf16_char;

          std::optional hex = hex_digit(*close_or_uchar);
          if (not hex)
            return escape_err(BAD_ESCAPE::MALFORMED);
          utf16_char = (utf16_char << 4) | *hex;
          if (utf16_char > 0x10FFFF)
            return escape_err(BAD_ESCAPE::MALFORMED);
        } while (true);
      } else {
        std::uint32_t high_surr = 0;
        for (int i = 0; i < 4; i++) {
          std::optional hex = next_uchar32(src_view).and_then(hex_digit);
          if (not hex)
            return escape_err(BAD_ESCAPE::MALFORMED);
          high_surr = (high_surr << 4) | *hex;
        }

        if (is_hi_surrogate(high_surr) && esc_rule == ESC_RULE::REGEXP_UTF16 &&
            src_view.starts_with("\\u"_u32)) {
          src_view = src_view | std::views::drop(2);
          std::uint32_t low_surr = 0;
          for (int i = 0; i < 4; i++) {
            std::optional hex = next_uchar32(src_view).and_then(hex_digit);
            if (not hex)
              return high_surr;
            low_surr = (low_surr << 4) | *hex;
          }
          if (is_lo_surrogate(low_surr))
            return from_surrogate(high_surr, low_surr);
        }

        return high_surr;
      }
    }

    case '0':
      if (esc_rule == ESC_RULE::REGEXP_UTF16) {
        if (next_uchar32(src_view)
                .transform([](UChar32 uch) { return std::isdigit(uch); })
                .value_or(false))
          return escape_err(BAD_ESCAPE::MALFORMED);
        return 0;
      }
      [[fallthrough]];

    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7': {
      UChar32 octal = *head - '0';
      std::optional<UChar32> ahead;
      ahead = next_uchar32(src_view).transform(
          [](UChar32 ahead_digit) { return ahead_digit - '0'; });
      if (not ahead || *ahead > 7)
        return octal;
      octal = (octal << 3) | *ahead;

      if (octal >= 32)
        return octal;

      ahead = next_uchar32(src_view).transform(
          [](UChar32 ahead_digit) { return ahead_digit - '0'; });
      if (not ahead || *ahead > 7)
        return octal;
      octal = (octal << 3) | *ahead;
      return octal;
    }

    default:
      return escape_err(BAD_ESCAPE::MISMATCH);
  }
}

bool match_identifier(u32_string_view lhs_view, u32_string_view rhs_view) {
  if (not lhs_view.starts_with(rhs_view))
    return false;
  lhs_view.remove_prefix(rhs_view.size());
  if (lhs_view.empty())
    return true;
  return !u_hasBinaryProperty(lhs_view.front(), UCHAR_XID_CONTINUE);
}

enum class TOKEN_AHEAD { ARROW, IN, IMPORT, OF, EXPORT, FUNCTION, IDENTIFIER };
enum class LINETERM_BEHAVIOR { RETURN, IGNORE };
using TokenAhead = std::variant<UChar32, TOKEN_AHEAD>;

template <LINETERM_BEHAVIOR LT>
std::optional<TokenAhead> peek_token(u32_string_view src_view) {
  do {
    if (src_view.empty())
      return std::nullopt;
    UChar32 ch = src_view.front();

    if (u_isWhitespace(ch)) {
      if constexpr (LT == LINETERM_BEHAVIOR::RETURN)
        switch (ch) {
          case '\r':
          case '\n':
          case 0x2028:
          case 0x2029:
            return '\n';
        }
      src_view = src_view | std::views::drop(1);
      continue;
    }

    if (src_view.starts_with("//"_u32)) {
      if constexpr (LT == LINETERM_BEHAVIOR::RETURN)
        return '\n';
      u32_string_view::iterator comment_end = std::ranges::find_if(
          src_view, [](char c) { return c != '\r' || c != '\n'; });
      src_view = u32_string_view{comment_end, src_view.end()};
      continue;
    }

    if (src_view.starts_with("/*"_u32)) {
      src_view = src_view | std::views::drop(2);
      while (next_uchar32(src_view)) {
        ch = src_view.front();
        if constexpr (LT == LINETERM_BEHAVIOR::RETURN)
          if (ch == '\r' || ch == '\n')
            return '\n';

        if (src_view.starts_with("*/"_u32)) {
          src_view = src_view | std::views::drop(2);
          break;
        }
      }
      continue;
    }

    if (src_view.starts_with("=>"_u32))
      return TOKEN_AHEAD::ARROW;

    using KeywordPair = std::pair<TOKEN_AHEAD, u32_string>;
    static const std::array keyword_arr =
        std::to_array<KeywordPair>({{TOKEN_AHEAD::IN, "in"_u32},
                                    {TOKEN_AHEAD::IMPORT, "import"_u32},
                                    {TOKEN_AHEAD::OF, "of"_u32},
                                    {TOKEN_AHEAD::EXPORT, "export"_u32},
                                    {TOKEN_AHEAD::FUNCTION, "function"_u32}});

    for (const KeywordPair& keyword_pair : keyword_arr)
      if (match_identifier(src_view, keyword_pair.second))
        return keyword_pair.first;

    if (src_view.starts_with("\\u"_u32)) {
      src_view = src_view | std::views::drop(1);
      if (parse_escape(src_view, ESC_RULE::IDENTIFIER)
              .transform([](UChar32 esc) {
                return u_hasBinaryProperty(esc, UCHAR_XID_START);
              })
              .value_or(false))
        return TOKEN_AHEAD::IDENTIFIER;
    }

    if (u_hasBinaryProperty(ch, UCHAR_XID_START))
      return TOKEN_AHEAD::IDENTIFIER;

    return ch;
  } while (true);
}

std::optional<TokenAhead> next_token(u32_string_view source_view) {
  std::optional parsed = peek_token<LINETERM_BEHAVIOR::RETURN>(source_view);
  if (not parsed)
    return std::nullopt;
  return *parsed;
}

u32_string new_u32_string(const std::string& narrow) {
  UErrorCode status = U_ZERO_ERROR;

  std::int32_t u16_size{};
  u_strFromUTF8(nullptr, 0, &u16_size, narrow.data(), -1, &status);
  if (status == U_BUFFER_OVERFLOW_ERROR)
    status = U_ZERO_ERROR;

  std::basic_string<UChar> medium{};
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

  u32_string wide{};
  wide.resize(u32_size);
  u_strToUTF32(wide.data(), u32_size + 1, nullptr, medium.data(), -1, &status);

  if (U_FAILURE(status))
    throw std::runtime_error{
        std::format("UTF-16 to UTF-32 failed: {}", u_errorName(status))};

  return wide;
}

namespace JS {
struct Mode {
  std::bitset<8> flags;
  bool is_strict() const { return flags[0]; }
};
}  // namespace JS

enum class BAD_STRING {
  UNEXPECTED_END,
  OCTAL_ESCAPE_IN_STRICT_MODE,
  MALFORMED_ESCAPE_SEQUENCE
};

std::expected<u32_string, BAD_STRING> parse_string_literal(
    const UChar32 sep,
    const JS::Mode js_mode,
    u32_string_view& src_view) {
  u32_string string_literal{};
  UChar32 ch;

  do {
    if (src_view.empty())
      return std::unexpected{BAD_STRING::UNEXPECTED_END};
    ch = src_view.front();

    if (sep == '`') {
      if (ch == '\r') {
        if (next_uchar32(src_view | std::views::drop(1)) == '\n')
          src_view = src_view | std::views::drop(1);
        ch = '\n';
      }
    } else if (ch == '\r' || ch == '\n')
      return std::unexpected{BAD_STRING::UNEXPECTED_END};

    src_view = src_view | std::views::drop(1);
    if (ch == sep)
      return string_literal;

    if (ch == '$' && next_uchar32(u32_string_view{src_view}) == '{' &&
        sep == '`') {
      src_view = src_view | std::views::drop(1);
      return string_literal;
    }

    if (ch == '\\') {
      if (src_view.empty())
        return std::unexpected{BAD_STRING::UNEXPECTED_END};
      ch = src_view.front();

      switch (ch) {
        case '\'':
        case '\"':
        case '\0':
        case '\\':
          src_view = src_view | std::views::drop(1);
          break;
        case '\r':
          if (next_uchar32(src_view | std::views::drop(1)) == '\n')
            src_view = src_view | std::views::drop(1);
          [[fallthrough]];
        case '\n':
        case 0x2028:
        case 0x2029:
          /* ignore escaped newline sequence */
          src_view = src_view | std::views::drop(1);
          continue;
        default:
          if (std::isdigit(ch) &&
              (ch != '0' ||
               next_uchar32(src_view | std::views::drop(1))
                   .transform([](UChar32 ahead) { return std::isdigit(ahead); })
                   .value_or(false))) {
            if (js_mode.is_strict() && (ch == '8' || ch == '9'))
              /* Note: according to ES2021, \8 and \9 are not
               * accepted in strict mode or in templates. */
              return std::unexpected{BAD_STRING::MALFORMED_ESCAPE_SEQUENCE};
            if (sep == '`')
              return std::unexpected{BAD_STRING::MALFORMED_ESCAPE_SEQUENCE};
            return std::unexpected{BAD_STRING::OCTAL_ESCAPE_IN_STRICT_MODE};
          }
          std::expected ch_exp = parse_escape(src_view, ESC_RULE::IDENTIFIER);
          if (ch_exp)
            ch = *ch_exp;
          else if (ch_exp.error() == BAD_ESCAPE::MISMATCH)
            /* ignore the '\' (could output a warning) */
            src_view = src_view | std::views::drop(1);
          else if (ch_exp.error() == BAD_ESCAPE::MALFORMED)
            return std::unexpected{BAD_STRING::MALFORMED_ESCAPE_SEQUENCE};
          break;
      }
    }
    string_literal.push_back(ch);
  } while (true);
}

}  // namespace Manadrain

export namespace Manadrain {

int parse_program(const std::string& source_str) {
  u32_string wide_str{new_u32_string(source_str)};
  if (next_token(wide_str))
    return 1;
  else
    return 0;
}

}  // namespace Manadrain
