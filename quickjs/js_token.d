module quickjs.js_token;
import quickjs;

import std.sumtype;

struct JSTokStr {
  JSValue str;
  int sep;
}

struct JSTokNum {
  JSValue num;
}

struct JSTokIdent {
  JSAtom atom;
  bool has_escape;
  bool is_reserved;
}

struct JSTokRegexp {
  JSValue body;
  JSValue flags;
}

enum JS_TOK {
  EOF, FUNCTION, IMPORT, EXPORT,
  ARROW, IN, OF
}

alias JSTokenVal = SumType!(
  JSTokStr, JSTokNum, JSTokIdent, JSTokRegexp,
  dchar, JS_TOK
);

struct JSToken {
  string pos;
  JSTokenVal val;
}

JSTokenVal simple_next_token(ref string pp, bool no_line_terminator) {
  import std.range.primitives;

  dchar take_then_pop() {
    scope(exit) pp.popFront;
    return pp.empty ? 0 : pp.front;
  }

  dchar pop_then_take() {
    pp.popFront;
    return pp.empty ? 0 : pp.front;
  }

  while (true) {
    switch (take_then_pop) {
      case '\r', '\n':
        if (no_line_terminator)
          return JSTokenVal('\n');
        continue;
      case ' ', '\t', '\v', '\f':
        continue;
      case '/':
        if (pp.front == '/') {
          if (no_line_terminator)
            return JSTokenVal('\n');
          while (!pp.empty && pp.front != '\r' && pp.front != '\n')
            pp.popFront;
          continue;
        }
        if (pp.front == '*') {
          while (pop_then_take) {
            if ((pp.front == '\r' || pp.front == '\n') && no_line_terminator)
              return JSTokenVal('\n');
            if (pp.match_identifier("*/")) {
              pp.popFrontExactly("*/".length);
              break;
            }
          }
          continue;
        }
        break;
      case '=':
        if (pp.front == '>')
          return JSTokenVal(JS_TOK.ARROW);
        break;
      case 'i':
        if (pp.front == 'n')
          return JSTokenVal(JS_TOK.IN);
        if (pp.match_identifier("mport")) {
          pp.popFrontExactly("mport".length);
          return JSTokenVal(JS_TOK.IMPORT);
        }
        return JSTokenVal(JSTokIdent());
      case 'o':
        if (pp.front == 'f')
          return JSTokenVal(JS_TOK.OF);
        return JSTokenVal(JSTokIdent());
      case 'e':
      default:
        break;
    }
    return JSTokenVal(pp.front);
  }
}

int match_identifier(string p, string s) {
  import std.range.primitives;
  if (p.length < s.length || p[0..s.length] != s)
    return 0;
  return !p.front.lre_js_is_ident_next;
}
