module quickjs:unicode;
import :utility;

namespace JS {
  namespace unicode {
    constexpr std::uint8_t UTF8_CHAR_LEN_MAX = 6;
    constexpr std::int32_t CP_LS = 0x2028;
    constexpr std::int32_t CP_PS = 0x2029;
  }
}

namespace JS {
  namespace unicode {
    std::int32_t from_hex(std::int32_t c) {
      if (c >= '0' && c <= '9')
        return c - '0';
      else if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
      else if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
      else
        return -1;
    }

    bool is_hi_surrogate(std::uint32_t c) {
      return (c >> 10) == (0xD800 >> 10); // 0xD800-0xDBFF
    }

    bool is_lo_surrogate(std::uint32_t c) {
      return (c >> 10) == (0xDC00 >> 10); // 0xDC00-0xDFFF
    }

    std::uint32_t from_surrogate(std::uint32_t hi, std::uint32_t lo) {
      return 0x10000 + 0x400 * (hi - 0xD800) + (lo - 0xDC00);
    }
  }
}

namespace JS {
  namespace unicode {
    constexpr std::uint32_t utf8_min_code[5] = {
      0x80, 0x800, 0x10000, 0x00200000, 0x04000000,
    };

    constexpr std::uint8_t utf8_first_code_mask[5] = {
      0x1f, 0xf, 0x7, 0x3, 0x1,
    };

    std::int32_t from_utf8(
      utility::PaddedBuf& buf, std::size_t begin_idx,
      std::int32_t max_len, std::size_t& end_idx
    ) {
      auto p = std::next(buf.begin(), begin_idx);
      std::int32_t l, c = *p++;
      if (c < 0x80) {
        end_idx = std::distance(buf.begin(), p);
        return c;
      }
      switch(c) {
        case 0xc0: case 0xc1: case 0xc2: case 0xc3:
        case 0xc4: case 0xc5: case 0xc6: case 0xc7:
        case 0xc8: case 0xc9: case 0xca: case 0xcb:
        case 0xcc: case 0xcd: case 0xce: case 0xcf:
        case 0xd0: case 0xd1: case 0xd2: case 0xd3:
        case 0xd4: case 0xd5: case 0xd6: case 0xd7:
        case 0xd8: case 0xd9: case 0xda: case 0xdb:
        case 0xdc: case 0xdd: case 0xde: case 0xdf:
        l = 1; break;

        case 0xe0: case 0xe1: case 0xe2: case 0xe3:
        case 0xe4: case 0xe5: case 0xe6: case 0xe7:
        case 0xe8: case 0xe9: case 0xea: case 0xeb:
        case 0xec: case 0xed: case 0xee: case 0xef:
        l = 2; break;

        case 0xf0: case 0xf1: case 0xf2: case 0xf3:
        case 0xf4: case 0xf5: case 0xf6: case 0xf7:
        l = 3; break;

        case 0xf8: case 0xf9: case 0xfa: case 0xfb:
        l = 4; break;

        case 0xfc: case 0xfd:
        l = 5; break;

        default: return -1;
      }
      if (l > (max_len - 1))
        return -1;
      c &= utf8_first_code_mask[l - 1];
      for (std::int32_t i = 0; i < l; i++) {
        std::int32_t b = *p++;
        if (b < 0x80 || b >= 0xc0)
          return -1;
        c = (c << 6) | (b & 0x3f);
      }
      if (c < utf8_min_code[l - 1])
        return -1;
      end_idx = std::distance(buf.begin(), p);
      return c;
    }
  }
}

namespace JS {
  namespace lre {
    using namespace unicode;

    bool is_id_start_byte(std::int32_t c) {
      return std::isalpha(c) || c == '$' || c == '_';
    }

    bool is_id_continue_byte(std::int32_t c) {
      return std::isalnum(c) || c == '$' || c == '_';
    }

    std::int32_t parse_escape(
      utility::PaddedBuf& buf, std::size_t& begin_idx,
      int allow_utf16
    ) {
      auto p = std::next(buf.begin(), begin_idx);
      std::uint32_t c = *p++;

      switch(c) {
        case 'b': c = '\b';
        break;

        case 'f': c = '\f';
        break;

        case 'n': c = '\n';
        break;

        case 'r': c = '\r';
        break;

        case 't': c = '\t';
        break;

        case 'v': c = '\v';
        break;

        case 'x': {
          std::int32_t h0 = from_hex(*p++);
          if (h0 < 0)
            return -1;
          std::int32_t h1 = from_hex(*p++);
          if (h1 < 0)
            return -1;
          c = (h0 << 4) | h1;
        }
        break;

        case 'u': {
          std::int32_t h, i;

          if (*p == '{' && allow_utf16) {
            p++; c = 0;
            while (true) {
              h = from_hex(*p++);
              if (h < 0)
                return -1;
              c = (c << 4) | h;
              if (c > 0x10FFFF)
                return -1;
              if (*p == '}')
                break;
            }
            p++;
          } else {
            c = 0;
            for(i = 0; i < 4; i++) {
              h = from_hex(*p++);
              if (h < 0) {
                return -1;
              }
              c = (c << 4) | h;
            }
            if (
              is_hi_surrogate(c) && allow_utf16 == 2 &&
              p[0] == '\\' && p[1] == 'u'
            ) {
              std::uint32_t c1 = 0;
              for(i = 0; i < 4; i++) {
                h = from_hex(p[2 + i]);
                if (h < 0)
                  break;
                c1 = (c1 << 4) | h;
              }
              if (i == 4 && is_lo_surrogate(c1)) {
                p += 6;
                c = from_surrogate(c, c1);
              }
            }
          }
        }
        break;

        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7':
        c -= '0';
        if (allow_utf16 == 2) {
          if (c != 0 || std::isdigit(*p))
            return -1;
        } else {
          std::uint32_t v = *p - '0';
          if (v > 7)
            break;
          c = (c << 3) | v;
          p++;
          if (c >= 32)
            break;
          v = *p - '0';
          if (v > 7)
            break;
          c = (c << 3) | v;
          p++;
        }
        break;

        default: return -2;
      }

      begin_idx = std::distance(buf.begin(), p);
      return c;
    }
  }
}
