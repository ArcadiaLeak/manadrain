export module manadrain;
import std;

namespace Manadrain {
  struct Token {};

  using UcharPair = std::pair<std::uint32_t, std::string_view>;
  using UcharTriple = std::tuple<bool, std::uint32_t, std::string_view>;
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

  std::optional<UcharPair> parse_escape(
    UcharPair switch_pair, UTF16_MODE utf16_mode
  ) {
    switch (switch_pair.first) {
      case 'b': return UcharPair{'\b', switch_pair.second};
      case 'f': return UcharPair{'\f', switch_pair.second};
      case 'n': return UcharPair{'\n', switch_pair.second};
      case 'r': return UcharPair{'\r', switch_pair.second};
      case 't': return UcharPair{'\t', switch_pair.second};
      case 'v': return UcharPair{'\v', switch_pair.second};
      case 'x': return next_char(switch_pair.second)
        .and_then(hex_digit)
        .and_then([](UcharPair hex0_pair) -> std::optional<UcharPair> {
          auto [hex0, hex0_view] = hex0_pair;
          return next_char(hex0_view)
            .and_then(hex_digit)
            .and_then([hex0](UcharPair hex1_pair) -> std::optional<UcharPair> {
              auto [hex1, hex1_view] = hex1_pair;
              return UcharPair{(hex0 << 4) | hex1, hex1_view};
            });
        });
      case 'u': return next_char(switch_pair.second)
        .and_then([utf16_mode](UcharPair brace_pair) -> std::optional<UcharPair> {
          if (brace_pair.first == '{' && utf16_mode != UTF16_MODE::DISABLED) {
            UcharTriple utf16_triple{false, 0, brace_pair.second};

            ahead: if (std::get<0>(utf16_triple))
              return UcharPair{std::get<1>(utf16_triple), std::get<2>(utf16_triple)};

            std::optional<UcharTriple> utf16_triple_opt = std::make_optional<UcharPair>(
              std::get<1>(utf16_triple), std::get<2>(utf16_triple)
            ).and_then([](UcharPair utf16_pair) -> std::optional<UcharTriple> {
              auto [utf16_prev, utf16_view] = utf16_pair;
              return next_char(utf16_view).and_then([utf16_prev](UcharPair next_pair) -> std::optional<UcharTriple> {
                if (next_pair.first == '}')
                  return UcharTriple{true, utf16_prev, next_pair.second};

                return hex_digit(next_pair).and_then([utf16_prev](UcharPair hex_pair) -> std::optional<UcharTriple> {
                  std::uint32_t utf16_char = (utf16_prev << 4) | hex_pair.first;

                  if (utf16_char > 0x10FFFF)
                    return std::nullopt;

                  return UcharTriple{false, utf16_char, hex_pair.second};
                });
              });
            });

            if (utf16_triple_opt) {
              utf16_triple = *utf16_triple_opt;
              goto ahead;
            }

            return std::nullopt;
          }

          // TODO

          return std::nullopt;
        });
      default: return std::nullopt;
    }
  }

  std::optional<UcharPair> next_token(std::string_view source_view) {
    return next_char(source_view).and_then([](UcharPair source_pair) {
      return parse_escape(source_pair, UTF16_MODE::NORMAL);
    });
  }

  int parse_program(std::string_view source_view) {
    if (next_token(source_view))
      return 1;
    return 0;
  }
}
