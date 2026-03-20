module;
#include <unicode/uchar.h>
#include <unicode/utf8.h>

module manadrain:unicode;
import :utility;

namespace JS::unicode {
  constexpr std::int32_t CP_LS = 0x2028;
  constexpr std::int32_t CP_PS = 0x2029;

  std::int32_t parse_hex(std::int32_t c) {
    if (c >= '0' && c <= '9')
      return c - '0';
    else if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    else if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    else throw std::runtime_error{
      "not a hex digit!"
    };
  }

  std::int32_t parse_uchar(
    utility::PaddedBuf& buf, std::size_t begin_idx, std::size_t& idx
  ) {
    UChar32 ch; U8_NEXT(buf, idx, 6, ch);
    if (ch == U_SENTINEL) throw std::runtime_error{"illegal UTF-8 sequence!"};
    return ch;
  }

  bool is_id_start_byte(std::int32_t c) {
    return u_isalpha(c) || c == '$' || c == '_';
  }

  bool is_id_continue_byte(std::int32_t c) {
    return u_isalnum(c) || c == '$' || c == '_';
  }

  std::int32_t parse_escape(utility::PaddedBuf& buf, std::size_t& begin_idx) {
    auto ptr = std::next(buf.begin(), begin_idx);
    std::uint32_t ch = *ptr++;

    switch(ch) {
      case 'b': ch = '\b'; break; case 'f': ch = '\f'; break;
      case 'n': ch = '\n'; break; case 'r': ch = '\r'; break;
      case 't': ch = '\t'; break; case 'v': ch = '\v'; break;

      case 'x': {
        std::int32_t hex0 = parse_hex(*ptr++);
        std::int32_t hex1 = parse_hex(*ptr++);
        ch = (hex0 << 4) | hex1;
        break;
      }

      case 'u':
      if (*ptr == '{') {
        ch = 0;
        while (*(++ptr) != '}') {
          ch = (ch << 4) | parse_hex(*ptr);
          if (ch > UCHAR_MAX_VALUE) throw std::runtime_error{
            "uchar overflow!"
          };
        }
        break;
      }
      ch = 0;
      for (int i = 0; i < 4; i++) {
        ch = (ch << 4) | parse_hex(*ptr++);
      }
      break;

      case '0': case '1': case '2': case '3':
      case '4': case '5': case '6': case '7': {
        /* parse a legacy octal sequence */
        std::uint32_t digit{};
        digit = *ptr - '0';
        ch = digit;
        if (digit > 7) break;
        ch = (ch << 3) | digit;
        ptr++;
        if (ch >= 32) break;
        digit = *ptr - '0';
        if (digit > 7) break;
        ch = (ch << 3) | digit;
        ptr++; break;
      }

      default:
      throw std::runtime_error{
        "illegal escape sequence!"
      };
    }

    begin_idx = std::distance(buf.begin(), ptr);
    return ch;
  }
}
