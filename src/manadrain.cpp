module;
#include <unicode/uchar.h>
#include <unicode/ustring.h>
export module manadrain;
import std;

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

std::optional<char32_t> next_uchar32(std::u32string_view& src_view) {
  if (src_view.empty())
    return std::nullopt;
  char32_t ch = src_view.front();
  src_view = src_view | std::views::drop(1);
  return ch;
}

std::optional<char32_t> next_uchar32(std::u32string_view&& src_view) {
  return next_uchar32(src_view);
}

std::optional<char32_t> hex_digit(char32_t digit) {
  if (digit >= '0' && digit <= '9')
    return digit - '0';
  if (digit >= 'A' && digit <= 'F')
    return digit - 'A' + 10;
  if (digit >= 'a' && digit <= 'f')
    return digit - 'a' + 10;
  return std::nullopt;
}

enum class BAD_ESCAPE { MALFORMED, PER_SE_BACKSLASH, OCTAL_SEQ };
enum class ESC_RULE {
  IDENTIFIER,
  REGEXP_ASCII,
  REGEXP_UTF16,
  STRING_IN_SLOPPY_MODE,
  STRING_IN_STRICT_MODE,
  STRING_IN_TEMPLATE
};

template <typename T>
std::unexpected<T> return_err_and_rewind(
    std::u32string_view& src_view,
    const std::u32string_view original_src_view,
    T error_tag) {
  src_view = original_src_view;
  return std::unexpected{error_tag};
}

std::expected<char32_t, BAD_ESCAPE> parse_escape(std::u32string_view& src_view,
                                                ESC_RULE esc_rule) {
  const std::u32string_view outer_src_view{src_view};

  std::optional head = next_uchar32(src_view);
  if (not head)
    return return_err_and_rewind(src_view, outer_src_view,
                                 BAD_ESCAPE::PER_SE_BACKSLASH);

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
        return return_err_and_rewind(src_view, outer_src_view,
                                     BAD_ESCAPE::MALFORMED);
      std::optional hex1 = next_uchar32(src_view).and_then(hex_digit);
      if (not hex1)
        return return_err_and_rewind(src_view, outer_src_view,
                                     BAD_ESCAPE::MALFORMED);
      return (*hex0 << 4) | *hex1;
    }

    case 'u': {
      std::optional open_or_uchar = next_uchar32(src_view);
      if (not open_or_uchar)
        return return_err_and_rewind(src_view, outer_src_view,
                                     BAD_ESCAPE::MALFORMED);
      if (open_or_uchar == '{' && esc_rule != ESC_RULE::REGEXP_ASCII) {
        char32_t utf16_char = 0;

        do {
          std::optional close_or_uchar = next_uchar32(src_view);
          if (not close_or_uchar)
            return return_err_and_rewind(src_view, outer_src_view,
                                         BAD_ESCAPE::MALFORMED);
          if (close_or_uchar == '}')
            return utf16_char;

          std::optional hex = hex_digit(*close_or_uchar);
          if (not hex)
            return return_err_and_rewind(src_view, outer_src_view,
                                         BAD_ESCAPE::MALFORMED);
          utf16_char = (utf16_char << 4) | *hex;
          if (utf16_char > 0x10FFFF)
            return return_err_and_rewind(src_view, outer_src_view,
                                         BAD_ESCAPE::MALFORMED);
        } while (true);
      } else {
        char32_t high_surr = 0;
        for (int i = 0; i < 4; i++) {
          std::optional hex = next_uchar32(src_view).and_then(hex_digit);
          if (not hex)
            return return_err_and_rewind(src_view, outer_src_view,
                                         BAD_ESCAPE::MALFORMED);
          high_surr = (high_surr << 4) | *hex;
        }

        if (is_hi_surrogate(high_surr) && esc_rule == ESC_RULE::REGEXP_UTF16 &&
            src_view.starts_with(U"\\u")) {
          src_view = src_view | std::views::drop(2);
          char32_t low_surr = 0;
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
      if (not next_uchar32(std::u32string_view{src_view})
                  .transform([](char32_t uch) { return std::isdigit(uch); })
                  .value_or(false))
        return 0;
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
          return return_err_and_rewind(src_view, outer_src_view,
                                       BAD_ESCAPE::OCTAL_SEQ);
        case ESC_RULE::STRING_IN_TEMPLATE:
        case ESC_RULE::REGEXP_UTF16:
          return return_err_and_rewind(src_view, outer_src_view,
                                       BAD_ESCAPE::MALFORMED);
        default:
          char32_t octal = *head - '0';
          std::optional<char32_t> ahead;
          ahead = next_uchar32(src_view).transform(
              [](char32_t ahead_digit) { return ahead_digit - '0'; });
          if (not ahead || *ahead > 7)
            return octal;
          octal = (octal << 3) | *ahead;

          if (octal >= 32)
            return octal;

          ahead = next_uchar32(src_view).transform(
              [](char32_t ahead_digit) { return ahead_digit - '0'; });
          if (not ahead || *ahead > 7)
            return octal;
          octal = (octal << 3) | *ahead;
          return octal;
      }

    case '8':
    case '9':
      if (esc_rule == ESC_RULE::STRING_IN_STRICT_MODE ||
          esc_rule == ESC_RULE::STRING_IN_TEMPLATE)
        return return_err_and_rewind(src_view, outer_src_view,
                                     BAD_ESCAPE::MALFORMED);
      [[fallthrough]];

    default:
      return return_err_and_rewind(src_view, outer_src_view,
                                   BAD_ESCAPE::PER_SE_BACKSLASH);
  }
}

bool match_identifier(std::u32string_view lhs_view, std::u32string_view rhs_view) {
  if (not lhs_view.starts_with(rhs_view))
    return false;
  lhs_view.remove_prefix(rhs_view.size());
  if (lhs_view.empty())
    return true;
  return !u_hasBinaryProperty(lhs_view.front(), UCHAR_XID_CONTINUE);
}

enum class TOKEN_AHEAD { ARROW, IN, IMPORT, OF, EXPORT, FUNCTION, IDENTIFIER };
enum class LINETERM_BEHAVIOR { RETURN, IGNORE };
using TokenAhead = std::variant<char32_t, TOKEN_AHEAD>;

template <LINETERM_BEHAVIOR LT>
std::optional<TokenAhead> peek_token(std::u32string_view src_view) {
  do {
    if (src_view.empty())
      return std::nullopt;
    char32_t ch = src_view.front();

    if (u_isWhitespace(ch)) {
      if constexpr (LT == LINETERM_BEHAVIOR::RETURN)
        switch (ch) {
          case '\r':
          case '\n':
          case 0x2028:
          case 0x2029:
            return U'\n';
        }
      src_view = src_view | std::views::drop(1);
      continue;
    }

    if (src_view.starts_with(U"//")) {
      if constexpr (LT == LINETERM_BEHAVIOR::RETURN)
        return U'\n';
      std::u32string_view::iterator comment_end = std::ranges::find_if(
          src_view, [](char c) { return c != '\r' || c != '\n'; });
      src_view = std::u32string_view{comment_end, src_view.end()};
      continue;
    }

    if (src_view.starts_with(U"/*")) {
      src_view = src_view | std::views::drop(2);
      while (next_uchar32(src_view)) {
        ch = src_view.front();
        if constexpr (LT == LINETERM_BEHAVIOR::RETURN)
          if (ch == '\r' || ch == '\n')
            return U'\n';

        if (src_view.starts_with(U"*/")) {
          src_view = src_view | std::views::drop(2);
          break;
        }
      }
      continue;
    }

    if (src_view.starts_with(U"=>"))
      return TOKEN_AHEAD::ARROW;

    using KeywordPair = std::pair<TOKEN_AHEAD, std::u32string>;
    static const std::array keyword_arr =
        std::to_array<KeywordPair>({{TOKEN_AHEAD::IN, U"in"},
                                    {TOKEN_AHEAD::IMPORT, U"import"},
                                    {TOKEN_AHEAD::OF, U"of"},
                                    {TOKEN_AHEAD::EXPORT, U"export"},
                                    {TOKEN_AHEAD::FUNCTION, U"function"}});

    for (const KeywordPair& keyword_pair : keyword_arr)
      if (match_identifier(src_view, keyword_pair.second))
        return keyword_pair.first;

    if (src_view.starts_with(U"\\u")) {
      src_view = src_view | std::views::drop(1);
      if (parse_escape(src_view, ESC_RULE::IDENTIFIER)
              .transform([](char32_t esc) {
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

std::optional<TokenAhead> next_token(std::u32string_view source_view) {
  std::optional parsed = peek_token<LINETERM_BEHAVIOR::RETURN>(source_view);
  if (not parsed)
    return std::nullopt;
  return *parsed;
}

std::u32string utf32_convert(const std::string& narrow) {
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

  return std::u32string{std::from_range,
                        wide | std::views::transform([](UChar32 uchar) {
                          return static_cast<char32_t>(uchar);
                        })};
}

enum class STRICTNESS { SLOPPY, STRICT };
enum class BAD_STRING {
  UNEXPECTED_END,
  OCTAL_SEQ_IN_ESCAPE,
  MALFORMED_SEQ_IN_ESCAPE
};

std::optional<BAD_STRING> parse_escaped_uchar_in_string(
    const char32_t quote,
    const STRICTNESS strictness,
    std::u32string_view& src_view,
    char32_t& ch,
    bool& must_continue) {
  if (src_view.empty())
    return BAD_STRING::UNEXPECTED_END;
  ch = src_view.front();
  switch (ch) {
    case '\'':
    case '\"':
    case '\0':
    case '\\':
      src_view = src_view | std::views::drop(1);
      return std::nullopt;
    case '\r':
      if (next_uchar32(src_view | std::views::drop(1)) == '\n')
        src_view = src_view | std::views::drop(1);
      [[fallthrough]];
    case '\n':
    case 0x2028:
    case 0x2029:
      /* ignore escaped newline sequence */
      src_view = src_view | std::views::drop(1);
      must_continue = true;
      return std::nullopt;
    default:
      ESC_RULE esc_rule = ESC_RULE::STRING_IN_SLOPPY_MODE;
      if (strictness == STRICTNESS::STRICT)
        esc_rule = ESC_RULE::STRING_IN_STRICT_MODE;
      else if (quote == '`')
        esc_rule = ESC_RULE::STRING_IN_TEMPLATE;
      std::expected ch_exp = parse_escape(src_view, esc_rule);
      if (ch_exp)
        ch = *ch_exp;
      else if (ch_exp.error() == BAD_ESCAPE::MALFORMED)
        return BAD_STRING::MALFORMED_SEQ_IN_ESCAPE;
      else if (ch_exp.error() == BAD_ESCAPE::OCTAL_SEQ)
        return BAD_STRING::OCTAL_SEQ_IN_ESCAPE;
      else if (ch_exp.error() == BAD_ESCAPE::PER_SE_BACKSLASH)
        /* ignore the '\' (could output a warning) */
        src_view = src_view | std::views::drop(1);
      return std::nullopt;
  }
}

std::expected<std::u32string, BAD_STRING> parse_quoted_string(
    const char32_t quote,
    const STRICTNESS strictness,
    std::u32string_view& src_view) {
  const std::u32string_view outer_src_view{src_view};

  std::u32string quoted_string{};
  char32_t ch;
  do {
    if (src_view.empty())
      return return_err_and_rewind(src_view, outer_src_view,
                                   BAD_STRING::UNEXPECTED_END);
    ch = src_view.front();

    if (quote == '`') {
      if (ch == '\r') {
        if (next_uchar32(src_view | std::views::drop(1)) == '\n')
          src_view = src_view | std::views::drop(1);
        ch = '\n';
      }
    } else if (ch == '\r' || ch == '\n')
      return return_err_and_rewind(src_view, outer_src_view,
                                   BAD_STRING::UNEXPECTED_END);

    src_view = src_view | std::views::drop(1);
    if (ch == quote)
      return quoted_string;

    if (ch == '$' && next_uchar32(std::u32string_view{src_view}) == '{' &&
        quote == '`') {
      src_view = src_view | std::views::drop(1);
      return quoted_string;
    }

    if (ch == '\\') {
      bool must_continue = false;
      std::optional esc_error = parse_escaped_uchar_in_string(
          quote, strictness, src_view, ch, must_continue);
      if (esc_error)
        return return_err_and_rewind(src_view, outer_src_view, *esc_error);
      else if (must_continue)
        continue;
    }

    quoted_string.push_back(ch);
  } while (true);
}

}  // namespace Manadrain

export namespace Manadrain {

int parse_program(const std::string& source_str) {
  std::u32string wide_str{utf32_convert(source_str)};
  if (next_token(wide_str))
    return 1;
  else
    return 0;
}

}  // namespace Manadrain
