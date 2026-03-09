module quickjs.regexp;
import quickjs;

int from_hex(int c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  else if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  else if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  else
    throw new Exception("malformed hex!");
}

bool is_lo_surrogate(uint c) =>
  (c >> 10) == (0xDC00 >> 10);

bool is_hi_surrogate(uint c) =>
  (c >> 10) == (0xD800 >> 10);

uint from_surrogate(uint hi, uint lo) =>
  0x10000 + 0x400 * (hi - 0xD800) + (lo - 0xDC00);

bool is_digit(int c) =>
  c >= '0' && c <= '9';

int lre_parse_escape(ref StrInputBuf pp, int allow_utf16) {
  import std.range;
  StrInputBuf p = pp;
  dchar c = p.front;
  p.popFront;
  switch (c) {
    case 'b':
      c = '\b';
      break;
    case 'f':
      c = '\f';
      break;
    case 'n':
      c = '\n';
      break;
    case 'r':
      c = '\r';
      break;
    case 't':
      c = '\t';
      break;
    case 'v':
      c = '\v';
      break;
    case 'x':
      {
        int h0 = from_hex(p.frontThenPop);
        int h1 = from_hex(p.frontThenPop);
        c = (h0 << 4) | h1;
      }
      break;
    case 'u':
      {
        int h, i;
        uint c1;

        if (p.front == '{' && allow_utf16) {
          p.popFront;
          c = 0;
          while (true) {
            h = from_hex(p.frontThenPop);
            c = (c << 4) | h;
            assert(c <= dchar.max);
            if (p.front == '}')
              break;
          }
          p.popFront;
        } else {
          c = 0;
          for (i = 0; i < 4; i++) {
            h = from_hex(p.frontThenPop);
            c = (c << 4) | h;
          }
          if (
            is_hi_surrogate(c) && allow_utf16 == 2 &&
            p.front == '\\' && p.dropOne.front == 'u'
          ) {
            c1 = 0;
            for (i = 0; i < 4; i++) {
              h = from_hex(p.drop(2 + i).front);
              c1 = (c1 << 4) | h;
            }
            if (i == 4 && is_lo_surrogate(c1)) {
              p = p.drop(6);
              c = from_surrogate(c, c1);
            }
          }
        }
      }
      break;
    case '0': .. case '7':
      c -= '0';
      if (allow_utf16 == 2) {
        if (c != 0 || is_digit(p.front))
          throw new Exception("malformed escape!");
      } else {
        uint v = p.front - '0';
        if (v > 7)
          break;
        c = (c << 3) | v;
        p.popFront;
        if (c >= 32)
          break;
        v = p.front - '0';
        if (v > 7)
          break;
        c = (c << 3) | v;
        p.popFront;
      }
      break;
    default:
      throw new Exception("escape parse failed!");
  }
  pp = p;
  return c;
}
