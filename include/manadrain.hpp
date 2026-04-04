#include <memory>
#include <string>

namespace Manadrain {
enum class STRICTNESS { SLOPPY, STRICT };
enum class ESC_RULE {
  IDENTIFIER,
  REGEXP_ASCII,
  REGEXP_UTF16,
  STRING_IN_SLOPPY_MODE,
  STRING_IN_STRICT_MODE,
  STRING_IN_TEMPLATE
};

enum class BAD_ESCAPE { MALFORMED, PER_SE_BACKSLASH, OCTAL_SEQ };
enum class BAD_STRING {
  UNEXPECTED_END,
  OCTAL_SEQ_IN_ESCAPE,
  MALFORMED_SEQ_IN_ESCAPE,
  MISMATCH
};

struct TokenString {
  char32_t sep;
  std::shared_ptr<const std::string> content;
};

struct ParseState {
  std::uint32_t idx;
};

struct ParseDriver {
  const std::basic_string<std::uint8_t> buffer;
  ParseState state;

  std::optional<char32_t> peek();
  std::optional<char32_t> shift();
  std::u32string_view take(std::uint32_t count, std::u32string& buf);
  void drop(std::uint32_t count);

  bool parseHex(std::uint32_t& digit);
  bool parseHex_b(std::uint32_t& digit);

  bool parseEscape(ESC_RULE esc_rule, std::pair<char32_t, BAD_ESCAPE>& either);
  bool parseEscape_b(ESC_RULE esc_rule,
                     std::pair<char32_t, BAD_ESCAPE>& either);
  bool parseEscape_hex(std::pair<char32_t, BAD_ESCAPE>& either);
  bool parseEscape_uni(ESC_RULE esc_rule,
                       std::pair<char32_t, BAD_ESCAPE>& either);
  bool parseEscape_braceSeq(std::pair<char32_t, BAD_ESCAPE>& either);
  bool parseEscape_fixedSeq(ESC_RULE esc_rule,
                            std::pair<char32_t, BAD_ESCAPE>& either);

  bool parseString(STRICTNESS strictness,
                   std::pair<TokenString, BAD_STRING>& either);
  int parseString_escape(STRICTNESS strictness,
                         char32_t sep,
                         std::pair<char32_t, BAD_STRING>& either);
  int parseString_escape_b(STRICTNESS strictness,
                           char32_t sep,
                           std::pair<char32_t, BAD_STRING>& either);
};
}  // namespace Manadrain
