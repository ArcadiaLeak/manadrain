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
  string str;
  bool has_escape;
  bool is_reserved;
}

struct JSTokRegexp {
  JSValue body;
  JSValue flags;
}

enum JS_TOK {
  NUMBER = -128, STRING, TEMPLATE, IDENT, REGEXP, MUL_ASSIGN,
  DIV_ASSIGN, MOD_ASSIGN, PLUS_ASSIGN, MINUS_ASSIGN, SHL_ASSIGN,
  SAR_ASSIGN, SHR_ASSIGN, AND_ASSIGN, XOR_ASSIGN, OR_ASSIGN,
  POW_ASSIGN, LAND_ASSIGN, LOR_ASSIGN, DOUBLE_QUESTION_MARK_ASSIGN,
  DEC, INC, SHL, SAR, SHR, LT, LTE, GT, GTE, EQ, STRICT_EQ, NEQ,
  STRICT_NEQ, LAND, LOR, POW, ARROW, ELLIPSIS, DOUBLE_QUESTION_MARK,
  QUESTION_MARK_DOT, ERROR, PRIVATE_NAME, EOF, NULL, FALSE, TRUE,
  IF, ELSE, RETURN, VAR, THIS, DELETE, VOID, TYPEOF, NEW, IN, INSTANCEOF,
  DO, WHILE, FOR, BREAK, CONTINUE, SWITCH, CASE, DEFAULT, THROW, TRY,
  CATCH, FINALLY, FUNCTION, DEBUGGER, WITH, CLASS, CONST, ENUM, EXPORT,
  EXTENDS, IMPORT, SUPER, IMPLEMENTS, INTERFACE, LET, PACKAGE, PRIVATE,
  PROTECTED, PUBLIC, STATIC, YIELD, AWAIT, OF
}

alias JSTokenVal = SumType!(
  JSTokStr, JSTokNum, JSTokIdent,
  JSTokRegexp, uint
);

struct JSToken {
  StrInputBuf pos;
  JSTokenVal val;
}

int simple_next_token(ref StrInputBuf pp, bool no_line_terminator) {
  import std.range;
  StrInputBuf p = pp;

  while (true) {
    dchar c = p.frontThenPop;
    switch (c) {
      case '\r', '\n':
        if (no_line_terminator)
          return '\n';
        continue;
      case ' ', '\t', '\v', '\f':
        continue;
      case '/':
        if (p.front == '/') {
          if (no_line_terminator)
            return '\n';
          while (p.front && p.front != '\r' && p.front != '\n')
            p.popFront;
          continue;
        }
        if (p.front == '*') {
          while (p.popThenFront) {
            if ((p.front == '\r' || p.front == '\n') && no_line_terminator)
              return '\n';
            if (p.front == '*' && p.dropOne.front == '/') {
              p = p.drop(2);
              break;
            }
          }
          continue;
        }
        break;
      case '=':
        if (p.front == '>')
          return JS_TOK.ARROW;
        break;
      case 'i':
        if (p.match_identifier("n"))
          return JS_TOK.IN;
        if (p.match_identifier("mport")) {
          pp = p.drop(5);
          return JS_TOK.IMPORT;
        }
        return JS_TOK.IDENT;
      case 'o':
        if (p.front == 'f')
          return JS_TOK.OF;
        return JS_TOK.IDENT;
      case 'e':
        if (p.match_identifier("xport"))
          return JS_TOK.EXPORT;
        return JS_TOK.IDENT;
      case 'f':
        if (p.match_identifier("unction"))
          return JS_TOK.FUNCTION;
        return JS_TOK.IDENT;
      case '\\':
        if (p.front == 'u')
          if (p.lre_parse_escape(true).lre_js_is_ident_first)
            return JS_TOK.IDENT;
        break;
      default:
        assert(0);
        break;
    }
    return c;
  }
}

int match_identifier(StrInputBuf p, dstring s) {
  import std.range;
  import std.utf;

  if (p.take(s.length).array == s)
    return 0;
  return !p.front.lre_js_is_ident_next;
}
