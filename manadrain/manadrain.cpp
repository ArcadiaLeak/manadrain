export module manadrain;
import std;

namespace Manadrain {
  struct Token {};

  template<typename T>
  using ParsePair = std::optional<std::pair<T, std::string_view>>;
  using UcharPair = ParsePair<std::uint32_t>;

  
}

export namespace Manadrain {
  struct Parser {
    std::string source_str;
    bool got_line_feed;

    int Parse() {
      return parse_program(source_str);
    }

    int parse_program(std::string_view source_view) {
      if (next_token(source_view))
        return 1;

      return 0;
    }

    int next_token(std::string_view source_view) {
      parse_escape(source_view);

      return 0;
    }

    UcharPair next_char(std::string_view source_view) {
      if (source_view.empty())
        return std::nullopt;
      
      return UcharPair{{source_view.front(), source_view | std::views::drop(1)}};
    }
    
    UcharPair parse_escape(std::string_view switch_view);
  };
}

namespace Manadrain {
  std::optional<std::uint32_t> parse_hex_digit(std::uint32_t digit) {
      if (digit >= '0' && digit <= '9')
        return digit - '0';
      if (digit >= 'A' && digit <= 'F')
        return digit - 'A' + 10;
      if (digit >= 'a' && digit <= 'f')
        return digit - 'a' + 10;
      return std::nullopt;
  }

  UcharPair Parser::parse_escape(std::string_view switch_view) {
    return next_char(switch_view).and_then(
      [&](auto switch_pair) -> UcharPair {
        auto [ch, case_view] = switch_pair;

        switch (ch) {
          case 'b': return std::make_pair('\b', case_view);
          case 'f': return std::make_pair('\f', case_view);
          case 'n': return std::make_pair('\n', case_view);
          case 'r': return std::make_pair('\r', case_view);
          case 't': return std::make_pair('\t', case_view);
          case 'v': return std::make_pair('\v', case_view);
          case 'x': return next_char(case_view).and_then(
            [&](auto hex0_pair) -> UcharPair {
              auto [hex0, hex0_view] = hex0_pair;
              return next_char(hex0_view).and_then(
                [&](auto hex1_pair) -> UcharPair {
                  auto [hex1, hex1_view] = hex1_pair;
                  return std::make_pair((hex0 << 4) | hex1, hex1_view);
                }
              );
            }
          );
          default: return std::nullopt;
        }
      }
    );
  }
}
