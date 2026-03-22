export module manadrain;
import std;

namespace Manadrain {
  struct Token {};

  using UcharPair = std::pair<std::uint32_t, std::string_view>;

  bool is_hi_surrogate(std::uint32_t c) {
    return (c >> 10) == (0xD800 >> 10); // 0xD800-0xDBFF
  }

  bool is_lo_surrogate(std::uint32_t c) {
    return (c >> 10) == (0xDC00 >> 10); // 0xDC00-0xDFFF
  }

  std::uint32_t from_surrogate(std::uint32_t hi, std::uint32_t lo) {
    return 0x10000 + 0x400 * (hi - 0xD800) + (lo - 0xDC00);
  }

  enum class BAD_ESCAPE {
    MALFORMED,
    FELL_THROUGH
  };
}

export namespace Manadrain {
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
    UcharPair switch_pair, UTF16_MODE utf16_mode
  ) {
    auto [switch_char, switch_view] = switch_pair;
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
          std::uint32_t uni_char = 0;
          std::string_view uni_view = brace_pair->second;

          for (int i = 0; i < 4; i++) {
            std::optional<UcharPair> hex_pair_opt =
              next_char(uni_view).and_then(hex_digit);
            if (not hex_pair_opt)
              return std::unexpected{BAD_ESCAPE::MALFORMED};
            uni_char = (uni_char << 4) | hex_pair_opt->first;
            uni_view = hex_pair_opt->second;
          }

          if (
            is_hi_surrogate(uni_char) && utf16_mode == UTF16_MODE::REGEXP &&
            std::string_view{uni_view | std::views::take(2)} == "\\u"
          ) {
            std::uint32_t losu_char = 0;
            std::string_view losu_view = uni_view | std::views::drop(2);
            for (int i = 0; i < 4; i++) {
              std::optional<UcharPair> hex_pair_opt =
                next_char(losu_view).and_then(hex_digit);
              if (not hex_pair_opt)
                goto return_uni_char;
              losu_char = (losu_char << 4) | hex_pair_opt->first;
              losu_view = hex_pair_opt->second;
            }
            if (is_lo_surrogate(losu_char)) {
              std::uint32_t complete_char = from_surrogate(uni_char, losu_char);
              return UcharPair{complete_char, losu_view};
            }
          }

          return_uni_char:
          return UcharPair{uni_char, uni_view};
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
        std::uint32_t octal = switch_char - '0';
        return std::unexpected{BAD_ESCAPE::MALFORMED};
      }

      default: return std::unexpected{BAD_ESCAPE::FELL_THROUGH};
    }
  }

  std::optional<UcharPair> next_token(std::string_view source_view) {
    std::optional<UcharPair> source_pair = next_char(source_view);
    if (not source_pair)
      return std::nullopt;
    std::expected<UcharPair, BAD_ESCAPE> parsed =
      parse_escape(*source_pair, UTF16_MODE::NORMAL);
    if (not parsed)
      return std::nullopt;
    return *parsed;
  }

  int parse_program(std::string_view source_view) {
    if (next_token(source_view))
      return 1;
    return 0;
  }
}
