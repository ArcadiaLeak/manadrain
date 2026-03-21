export module manadrain;
import std;

namespace Manadrain {
  struct Token {};

  template<typename T>
  using ParsePair = std::optional<std::pair<T, std::string_view>>;
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

    using UcharPair = ParsePair<std::uint32_t>;
    
    UcharPair parse_escape(std::string_view switch_view) {
      using EscapeCase = std::pair<std::string_view, std::function<UcharPair()>>;

      static const std::unordered_map escape_switch = {
        EscapeCase{"key1", []() {
          return UcharPair{{0, "value_for_key1"}};
        }},
        EscapeCase{"key2", []() {
          return UcharPair{{0, "value_for_key2"}};
        }},
      };

      auto found = escape_switch.find(switch_view);
      if (found != escape_switch.end())
        return found->second();

      return std::nullopt;
    }
  };
}
