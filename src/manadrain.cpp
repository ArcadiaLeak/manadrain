module;
#include <unicode/uchar.h>
#include <unicode/utypes.h>

export module manadrain;
import std;

namespace Manadrain {
  bool is_hi_surrogate(std::uint32_t c) { return (c >> 10) == (0xD800 >> 10); /* 0xD800-0xDBFF */ }
  bool is_lo_surrogate(std::uint32_t c) { return (c >> 10) == (0xDC00 >> 10); /* 0xDC00-0xDFFF */ }
  std::uint32_t from_surrogate( std::uint32_t hi, std::uint32_t lo)
    { return 0x10000 + 0x400 * (hi - 0xD800) + (lo - 0xDC00); }

  enum class BAD_ESCAPE {
    MALFORMED,
    MISMATCH
  };

  template<typename T>
  using ParsePair = std::pair<T, std::string_view>;
  using UcharPair = ParsePair<std::uint32_t>;

  std::optional<UcharPair> next_char(std::string_view source_view) {
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

  std::expected<UcharPair, BAD_ESCAPE> parse_escape(
    std::string_view source_view, UTF16_MODE utf16_mode
  ) {
    std::optional<UcharPair> switch_pair = next_char(source_view);
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
        std::optional<UcharPair> hex0_pair =
          next_char(switch_view).and_then(hex_digit);
        if (not hex0_pair)
          return std::unexpected{BAD_ESCAPE::MALFORMED};
        auto [hex0, hex0_view] = *hex0_pair;
        std::optional<UcharPair> hex1_pair =
          next_char(hex0_view).and_then(hex_digit);
        if (not hex1_pair)
          return std::unexpected{BAD_ESCAPE::MALFORMED};
        auto [hex1, hex1_view] = *hex1_pair;
        return UcharPair{(hex0 << 4) | hex1, hex1_view}; 
      }

      case 'u': {
        std::optional<UcharPair> brace_pair = next_char(switch_view);
        if (not brace_pair)
          return std::unexpected{BAD_ESCAPE::MALFORMED};
        if (brace_pair->first == '{' && utf16_mode != UTF16_MODE::DISABLED) {
          std::uint32_t utf16_char = 0;
          std::string_view utf16_view = brace_pair->second;

          while (true) {
            std::optional<UcharPair> end_pair_opt = next_char(utf16_view);
            if (not end_pair_opt)
              return std::unexpected{BAD_ESCAPE::MALFORMED};
            if (end_pair_opt->first == '}')
              return UcharPair{utf16_char, end_pair_opt->second};

            std::optional<UcharPair> hex_pair_opt = hex_digit(*end_pair_opt);
            if (not hex_pair_opt)
              return std::unexpected{BAD_ESCAPE::MALFORMED};
            utf16_char = (utf16_char << 4) | hex_pair_opt->first;
            if (utf16_char > 0x10FFFF)
              return std::unexpected{BAD_ESCAPE::MALFORMED};
            utf16_view = hex_pair_opt->second;
          }
        } else {
          std::string_view uni_view = brace_pair->second;
          
          std::uint32_t high_surr = 0;
          for (int i = 0; i < 4; i++) {
            std::optional<UcharPair> hex_pair_opt =
              next_char(uni_view).and_then(hex_digit);
            if (not hex_pair_opt)
              return std::unexpected{BAD_ESCAPE::MALFORMED};
            high_surr = (high_surr << 4) | hex_pair_opt->first;
            uni_view = hex_pair_opt->second;
          }

          if (
            is_hi_surrogate(high_surr) && utf16_mode == UTF16_MODE::REGEXP &&
            uni_view.starts_with("\\u")
          ) {
            std::uint32_t low_surr = 0;
            uni_view.remove_prefix(2);
            for (int i = 0; i < 4; i++) {
              std::optional<UcharPair> hex_pair_opt =
                next_char(uni_view).and_then(hex_digit);
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
        std::optional<UcharPair> ahead_pair = next_char(switch_view);
        if (ahead_pair && std::isdigit(ahead_pair->first))
          return std::unexpected{BAD_ESCAPE::MALFORMED};
        return UcharPair{0, switch_view};
      }
      [[fallthrough]];

      case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': {
        UcharPair octal_pair{switch_char - '0', switch_view};
        std::optional<UcharPair> ahead_pair_opt{};

        ahead_pair_opt = next_char(octal_pair.second)
          .transform([](UcharPair ahead_pair) {
            return UcharPair{ahead_pair.first - '0', ahead_pair.second};
          });
        if (not ahead_pair_opt || ahead_pair_opt->first > 7)
          return octal_pair;
        octal_pair.first = (octal_pair.first << 3) | ahead_pair_opt->first;
        octal_pair.second = ahead_pair_opt->second;

        if (octal_pair.first >= 32)
          return octal_pair;

        ahead_pair_opt = next_char(octal_pair.second)
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

  /* Index: 0 -> 2-byte sequence, 1 -> 3-byte, ..., 4 -> 6-byte. */
  std::array min_code_for_len = std::to_array<std::uint32_t>
    ({ 0x80, 0x800, 0x10000, 0x00200000, 0x04000000 });

  /* Index: 0 -> keep 5 bits, 1 -> 4 bits, ..., 4 -> 1 bit. */
  std::array first_byte_mask = std::to_array<std::uint32_t>
    ({ 0x1F, 0x0F, 0x07, 0x03, 0x01 });

  int utf8_continuation(int first_byte) {
    if (first_byte >= 0xC0 && first_byte <= 0xDF)
      return 1;
    else if (first_byte >= 0xE0 && first_byte <= 0xEF)
      return 2;
    else if (first_byte >= 0xF0 && first_byte <= 0xF7)
      return 3;
    else if (first_byte >= 0xF8 && first_byte <= 0xFB)
      return 4;
    else if (first_byte >= 0xFC && first_byte <= 0xFD)
      return 5;
    return -1;
  }

  std::optional<UcharPair> unicode_from_utf8(std::string_view start_view) {
    std::optional<UcharPair> first_pair = next_char(start_view);
    if (not first_pair)
      return std::nullopt;

    int first_byte = first_pair->first;
    if (first_byte < 0x80)
      return first_pair;

    int cont_bytes = utf8_continuation(first_byte);
    if (cont_bytes == -1)
      return std::nullopt;

    UcharPair code_pair{
      first_byte & first_byte_mask[cont_bytes - 1],
      first_pair->second
    };

    for (int i = 0; i < cont_bytes; i++) {
      std::optional<UcharPair> cont_pair_opt = next_char(code_pair.second);
      /* Continuation bytes must be 10xxxxxx (0x80..0xBF) */
      if (not cont_pair_opt || cont_pair_opt->first < 0x80 || cont_pair_opt->first >= 0xC0)
        return std::nullopt;
      code_pair.first = (code_pair.first << 6) | (cont_pair_opt->first & 0x3F);
      code_pair.second = cont_pair_opt->second;
    }

    if (code_pair.first < min_code_for_len[cont_bytes - 1])
      return std::nullopt;

    return code_pair;
  }

  bool match_identifier(std::string_view lhs_view, std::string_view rhs_view) {
    if (not lhs_view.starts_with(rhs_view))
      return 0;
    lhs_view.remove_prefix(rhs_view.size());
    std::optional<UcharPair> ahead_pair_opt = unicode_from_utf8(lhs_view);
    if (not ahead_pair_opt)
      return 1;
    return !u_hasBinaryProperty(ahead_pair_opt->first, UCHAR_XID_CONTINUE);
  }

  using Tri = std::tuple<std::uint8_t, std::uint8_t, std::uint32_t>;
  using TriPair = ParsePair<Tri>;
  enum class LINE_FEED { RETURN, IGNORE };

  template<LINE_FEED LF>
  std::optional<TriPair> peek_token(std::string_view source_view) {
    while (true) {
      std::optional uchar_pair = next_char(source_view);

      if (uchar_pair && std::isspace(uchar_pair->first)) {
        source_view = uchar_pair->second;
        if ((uchar_pair->first == '\r' || uchar_pair->first == '\n') && LF == LINE_FEED::RETURN)
          return TriPair{{0, 0, '\n'}, source_view};
        continue;
      }

      if (source_view.starts_with("//")) {
        source_view.remove_prefix(2);
        if constexpr (LF == LINE_FEED::RETURN)
          return TriPair{{0, 0, '\n'}, source_view};
        std::string_view::iterator comment_end =
          std::ranges::find_if(source_view, [](char ch) { return ch != '\r' || ch != '\n'; });
        source_view = std::string_view{comment_end, source_view.end()};
        continue;
      }

      if (source_view.starts_with("/*")) {
        bool done = false; 
        skip_delim: source_view.remove_prefix(2);
        if (done) continue;

        skip_text: uchar_pair = next_char(source_view);
        if (not uchar_pair)
          return std::nullopt;

        source_view = uchar_pair->second;
        if ((uchar_pair->first == '\r' || uchar_pair->first == '\n') && LF == LINE_FEED::RETURN)
          return TriPair{{0, 0, '\n'}, source_view};
        if (source_view.starts_with("*/"))
          done = true;
        if (done) goto skip_delim; else goto skip_text;
      }

      return std::nullopt;
    }
  }

  std::optional<TriPair> next_token(std::string_view source_view) {
    std::optional<TriPair> parsed = peek_token<LINE_FEED::RETURN>(source_view);
    if (not parsed) return std::nullopt;
    return *parsed;
  }
}

export namespace Manadrain {
  int parse_program(std::string_view source_view) {
    if (next_token(source_view))
      return 1;
    return 0;
  }
}
