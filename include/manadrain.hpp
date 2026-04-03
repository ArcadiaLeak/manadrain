#include <string>

namespace Manadrain {
enum class BAD_ESCAPE { MALFORMED, PER_SE_BACKSLASH, OCTAL_SEQ };
enum class ESC_RULE {
  IDENTIFIER,
  REGEXP_ASCII,
  REGEXP_UTF16,
  STRING_IN_SLOPPY_MODE,
  STRING_IN_STRICT_MODE,
  STRING_IN_TEMPLATE
};

struct ParseState {
  std::uint32_t idx;
};

struct ParseDriver {
  std::basic_string<std::uint8_t> buffer;
  ParseState state;

  std::optional<char32_t> peek();
  std::optional<char32_t> shift();
  void drop(std::uint32_t count);

  bool parseHex(std::uint32_t& digit);
  bool parseHex_b(std::uint32_t& digit);

  bool parseEscape(ESC_RULE esc_rule, std::pair<char32_t, BAD_ESCAPE>& either);
  bool parseEscape_b(ESC_RULE esc_rule,
                     std::pair<char32_t, BAD_ESCAPE>& either);
};
}  // namespace Manadrain
