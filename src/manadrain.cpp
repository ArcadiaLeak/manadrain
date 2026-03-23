module;
#include <unicode/uchar.h>
#include <unicode/ustring.h>
export module manadrain;
import std;

namespace Manadrain {
  bool is_hi_surrogate(std::uint32_t c) { return (c >> 10) == (0xD800 >> 10); /* 0xD800-0xDBFF */ }
  bool is_lo_surrogate(std::uint32_t c) { return (c >> 10) == (0xDC00 >> 10); /* 0xDC00-0xDFFF */ }
  std::uint32_t from_surrogate(std::uint32_t hi, std::uint32_t lo)
    { return 0x10000 + 0x400 * (hi - 0xD800) + (lo - 0xDC00); }

  enum class BAD_ESCAPE {
    MALFORMED,
    MISMATCH
  };

  using u32_string = std::basic_string<UChar32>;
  using u32_string_view = std::basic_string_view<UChar32>;
  u32_string operator""_icu32(const char* str, std::size_t len) { return u32_string{str, str + len}; }

  using UcharPair = std::pair<std::uint32_t, u32_string_view>;
  std::optional<UcharPair> next_uchar32(u32_string_view source_view) {
    if (source_view.empty()) return std::nullopt;
    return UcharPair{source_view.front(), source_view | std::views::drop(1)};
  }

  std::optional<UcharPair> hex_digit(UcharPair source_pair) {
    auto [digit, source_view] = source_pair;
    if (digit >= '0' && digit <= '9')
      return UcharPair{digit - '0', source_view};
    if (digit >= 'A' && digit <= 'F')
      return UcharPair{digit - 'A' + 10, source_view};
    if (digit >= 'a' && digit <= 'f')
      return UcharPair{digit - 'a' + 10, source_view};
    return std::nullopt;
  }

  enum class UTF16_MODE {
    DISABLED,
    NORMAL,
    REGEXP
  };

  template<UTF16_MODE utf16_mode>
  std::expected<UcharPair, BAD_ESCAPE> parse_escape(u32_string_view source_view) {
    std::optional switch_pair = next_uchar32(source_view);
    if (not switch_pair) return std::unexpected{BAD_ESCAPE::MISMATCH};

    auto [switch_char, switch_view] = *switch_pair;
    switch (switch_char) {
      case 'b': return UcharPair{'\b', switch_view};
      case 'f': return UcharPair{'\f', switch_view};
      case 'n': return UcharPair{'\n', switch_view};
      case 'r': return UcharPair{'\r', switch_view};
      case 't': return UcharPair{'\t', switch_view};
      case 'v': return UcharPair{'\v', switch_view};

      case 'x': {
        std::optional hex0_pair = next_uchar32(switch_view).and_then(hex_digit);
        if (not hex0_pair)
          return std::unexpected{BAD_ESCAPE::MALFORMED};
        auto [hex0, hex0_view] = *hex0_pair;
        std::optional hex1_pair = next_uchar32(hex0_view).and_then(hex_digit);
        if (not hex1_pair)
          return std::unexpected{BAD_ESCAPE::MALFORMED};
        auto [hex1, hex1_view] = *hex1_pair;
        return UcharPair{(hex0 << 4) | hex1, hex1_view}; 
      }

      case 'u': {
        std::optional brace_pair = next_uchar32(switch_view);
        if (not brace_pair)
          return std::unexpected{BAD_ESCAPE::MALFORMED};
        if (brace_pair->first == '{' && utf16_mode != UTF16_MODE::DISABLED) {
          std::uint32_t utf16_char = 0;
          u32_string_view utf16_view = brace_pair->second;

          while (true) {
            std::optional end_pair_opt = next_uchar32(utf16_view);
            if (not end_pair_opt)
              return std::unexpected{BAD_ESCAPE::MALFORMED};
            if (end_pair_opt->first == '}')
              return UcharPair{utf16_char, end_pair_opt->second};

            std::optional hex_pair_opt = hex_digit(*end_pair_opt);
            if (not hex_pair_opt)
              return std::unexpected{BAD_ESCAPE::MALFORMED};
            utf16_char = (utf16_char << 4) | hex_pair_opt->first;
            if (utf16_char > 0x10FFFF)
              return std::unexpected{BAD_ESCAPE::MALFORMED};
            utf16_view = hex_pair_opt->second;
          }
        } else {
          u32_string_view uni_view = brace_pair->second;
          
          std::uint32_t high_surr = 0;
          for (int i = 0; i < 4; i++) {
            std::optional hex_pair_opt = next_uchar32(uni_view).and_then(hex_digit);
            if (not hex_pair_opt)
              return std::unexpected{BAD_ESCAPE::MALFORMED};
            high_surr = (high_surr << 4) | hex_pair_opt->first;
            uni_view = hex_pair_opt->second;
          }

          if (
            is_hi_surrogate(high_surr) && utf16_mode == UTF16_MODE::REGEXP &&
            uni_view.starts_with("\\u"_icu32)
          ) {
            std::uint32_t low_surr = 0;
            uni_view.remove_prefix(2);
            for (int i = 0; i < 4; i++) {
              std::optional hex_pair_opt = next_uchar32(uni_view).and_then(hex_digit);
              if (not hex_pair_opt)
                goto return_high_surr;
              low_surr = (low_surr << 4) | hex_pair_opt->first;
              uni_view = hex_pair_opt->second;
            }
            if (is_lo_surrogate(low_surr)) {
              std::uint32_t uni_char = from_surrogate(high_surr, low_surr);
              return UcharPair{uni_char, uni_view};
            }
          }

          return_high_surr:
          return UcharPair{high_surr, uni_view};
        }
      }

      case '0':
      if (utf16_mode == UTF16_MODE::REGEXP) {
        std::optional ahead_pair = next_uchar32(switch_view);
        if (ahead_pair && std::isdigit(ahead_pair->first))
          return std::unexpected{BAD_ESCAPE::MALFORMED};
        return UcharPair{0, switch_view};
      }
      [[fallthrough]];

      case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': {
        UcharPair octal_pair{switch_char - '0', switch_view};
        std::optional<UcharPair> ahead_pair_opt;

        ahead_pair_opt = next_uchar32(octal_pair.second)
          .transform([](UcharPair ahead_pair) {
            return UcharPair{ahead_pair.first - '0', ahead_pair.second};
          });
        if (not ahead_pair_opt || ahead_pair_opt->first > 7)
          return octal_pair;
        octal_pair.first = (octal_pair.first << 3) | ahead_pair_opt->first;
        octal_pair.second = ahead_pair_opt->second;

        if (octal_pair.first >= 32)
          return octal_pair;

        ahead_pair_opt = next_uchar32(octal_pair.second)
          .transform([](UcharPair ahead_pair) {
            return UcharPair{ahead_pair.first - '0', ahead_pair.second};
          });
        if (not ahead_pair_opt || ahead_pair_opt->first > 7)
          return octal_pair;
        octal_pair.first = (octal_pair.first << 3) | ahead_pair_opt->first;
        octal_pair.second = ahead_pair_opt->second;
        return octal_pair;
      }

      default: return std::unexpected{BAD_ESCAPE::MISMATCH};
    }
  }

  bool match_identifier(u32_string_view lhs_view, u32_string_view rhs_view) {
    if (not lhs_view.starts_with(rhs_view))
      return 0;
    lhs_view.remove_prefix(rhs_view.size());
    std::optional ahead_pair_opt = next_uchar32(lhs_view);
    if (not ahead_pair_opt)
      return 1;
    return !u_hasBinaryProperty(ahead_pair_opt->first, UCHAR_XID_CONTINUE);
  }

  enum class PEEK_TYPE { ARROW, IN, IMPORT, OF, EXPORT, FUNCTION, IDENTIFIER };
  enum class PEEK_LF { RETURN, IGNORE };
  using PeekedToken = std::variant<std::uint32_t, PEEK_TYPE>;

  template<PEEK_LF LF>
  std::optional<PeekedToken> peek_token(u32_string_view source_view) {
    std::optional<UcharPair> ch;

    again: ch = next_uchar32(source_view);
    if (not ch)
      return std::nullopt;

    if (u_isWhitespace(ch->first)) {
      if constexpr (LF == PEEK_LF::RETURN) switch (ch->first) {
        case '\r': case '\n': case 0x2028: case 0x2029:
        return std::uint32_t{'\n'};
      }
      source_view = ch->second;
      goto again;
    }

    if (source_view.starts_with("//"_icu32)) {
      if constexpr (LF == PEEK_LF::RETURN)
        return std::uint32_t{'\n'};
      u32_string_view::iterator comment_end =
        std::ranges::find_if(source_view, [](char c) { return c != '\r' || c != '\n'; });
      source_view = u32_string_view{comment_end, source_view.end()};
      goto again;
    }

    if (source_view.starts_with("/*"_icu32)) {
      bool done = false; 
      skip_delim: source_view.remove_prefix(2);
      if (done) goto again;

      skip_text: ch = next_uchar32(source_view);
      if (not ch)
        return std::nullopt;

      source_view = ch->second;
      if constexpr (LF == PEEK_LF::RETURN) if (ch->first == '\r' || ch->first == '\n')
        return std::uint32_t{'\n'};
      if (source_view.starts_with("*/"_icu32))
        done = true;
      if (done) goto skip_delim; else goto skip_text;
    }

    if (source_view.starts_with("=>"_icu32))
      return PEEK_TYPE::ARROW;

    using KeywordPair = std::pair<PEEK_TYPE, u32_string>;
    static const std::array keyword_arr = std::to_array<KeywordPair>({
      { PEEK_TYPE::IN, "in"_icu32 },
      { PEEK_TYPE::IMPORT, "import"_icu32 },
      { PEEK_TYPE::OF, "of"_icu32 },
      { PEEK_TYPE::EXPORT, "export"_icu32 },
      { PEEK_TYPE::FUNCTION, "function"_icu32 }
    });
    for (const KeywordPair& keyword_pair : keyword_arr)
      if (match_identifier(source_view, keyword_pair.second))
        return keyword_pair.first;

    if (source_view.starts_with("\\u"_icu32)) {
      std::expected escape = parse_escape<UTF16_MODE::NORMAL>(source_view);
      if (not escape)
        return std::nullopt;
      ch = *escape;
    }

    if (u_hasBinaryProperty(ch->first, UCHAR_XID_START))
      return PEEK_TYPE::IDENTIFIER;

    return ch->first;
  }

  std::optional<PeekedToken> next_token(u32_string_view source_view) {
    std::optional parsed = peek_token<PEEK_LF::RETURN>(source_view);
    if (not parsed) return std::nullopt;
    return *parsed;
  }

  u32_string conv_narrow_str(std::string narrow) {
    UErrorCode status = U_ZERO_ERROR;

    std::int32_t u16_size{};
    u_strFromUTF8(nullptr, 0, &u16_size, narrow.data(), -1, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) status = U_ZERO_ERROR;
    
    std::basic_string<UChar> medium{}; medium.resize(u16_size);
    u_strFromUTF8(medium.data(), u16_size + 1, nullptr, narrow.data(), -1, &status);

    if (U_FAILURE(status))
      throw std::runtime_error{
        std::format("UTF-8 to UTF-16 failed: {}", u_errorName(status))
      };

    std::int32_t u32_size{};
    u_strToUTF32(nullptr, 0, &u32_size, medium.data(), -1, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) status = U_ZERO_ERROR;
    
    u32_string wide{}; wide.resize(u32_size);
    u_strToUTF32(wide.data(), u32_size + 1, nullptr, medium.data(), -1, &status);

    if (U_FAILURE(status))
      throw std::runtime_error{
        std::format("UTF-16 to UTF-32 failed: {}", u_errorName(status))
      };

    return wide;
  }
}

export namespace Manadrain {
  int parse_program(std::string source_str) {
    std::basic_string wide_str = conv_narrow_str(source_str);
    if (next_token(wide_str))
      return 1;
    return 0;
  }
}
