export module manadrain;
import std;

namespace Manadrain {
  struct Token {};

  template<typename T>
  using ParsePair = std::optional<std::pair<T, std::string_view>>;
  using UcharPair = ParsePair<std::uint32_t>;
}

export namespace Manadrain {
  UcharPair next_char(std::string_view source_view) {
    if (source_view.empty()) return std::nullopt;
    return UcharPair{{source_view.front(), source_view | std::views::drop(1)}};
  }
  
  UcharPair parse_digit16(auto source_pair) {
    auto [digit, source_view] = source_pair;
    if (digit >= '0' && digit <= '9')
      return std::make_pair(digit - '0', source_view);
    if (digit >= 'A' && digit <= 'F')
      return std::make_pair(digit - 'A' + 10, source_view);
    if (digit >= 'a' && digit <= 'f')
      return std::make_pair(digit - 'a' + 10, source_view);
    return std::nullopt;
  }

  UcharPair parse_escape(std::string_view switch_view) {
    return next_char(switch_view).and_then(
      [](auto switch_pair) -> UcharPair {
        auto [ch, case_view] = switch_pair;

        switch (ch) {
          case 'b': return std::make_pair('\b', case_view);
          case 'f': return std::make_pair('\f', case_view);
          case 'n': return std::make_pair('\n', case_view);
          case 'r': return std::make_pair('\r', case_view);
          case 't': return std::make_pair('\t', case_view);
          case 'v': return std::make_pair('\v', case_view);
          case 'x': return next_char(case_view)
            .and_then([](auto char_pair) { return parse_digit16(char_pair); })
            .and_then([](auto hex0_pair) -> UcharPair {
              auto [hex0, hex0_view] = hex0_pair;
              return next_char(hex0_view)
                .and_then([](auto char_pair) { return parse_digit16(char_pair); })
                .and_then([hex0](auto hex1_pair) -> UcharPair {
                  auto [hex1, hex1_view] = hex1_pair;
                  return std::make_pair((hex0 << 4) | hex1, hex1_view);
                });
            });
          default: return std::nullopt;
        }
      }
    );
  }

  UcharPair next_token(std::string_view source_view) {
    return parse_escape(source_view);;
  }

  int parse_program(std::string_view source_view) {
    if (next_token(source_view))
      return 1;
    return 0;
  }
}
