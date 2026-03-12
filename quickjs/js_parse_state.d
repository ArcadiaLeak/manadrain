module quickjs.js_parse_state;
import quickjs;

enum JS_PARSE_FUNC {
  STATEMENT, VAR, EXPR, ARROW, GETTER, SETTER, METHOD,
  CLASS_STATIC_INIT, CLASS_CONSTRUCTOR, DERIVED_CLASS_CONSTRUCTOR
}

enum JS_PARSE_EXPORT {
  NONE, NAMED, DEFAULT
}

enum JS_MODE_STRICT = 1 << 0;
enum JS_MODE_ASYNC = 1 << 2;
enum JS_MODE_BACKTRACE_BARRIER = 1 << 3;

enum JS_FUNC_NORMAL = 0;
enum JS_FUNC_GENERATOR = 1 << 0;
enum JS_FUNC_ASYNC = 1 << 1;
enum JS_FUNC_ASYNC_GENERATOR = JS_FUNC_GENERATOR | JS_FUNC_ASYNC;

struct StrOutputBuf {
  import std.container.dlist;

  struct Chunk {
    dchar[ubyte.max] data;
    ubyte length;
  }

  DList!Chunk chunks;

  void put(dchar elem) {
    if (chunks.empty || chunks.back.length == ubyte.max)
      chunks.insert = Chunk();
    chunks.back.data[chunks.back.length++] = elem;
  }

  size_t length() {
    size_t len;
    foreach (ref chunk; chunks)
      len += chunk.length;
    return len;
  }
}

struct StrInputBuf {
  string str;

  enum bool empty = false;
  StrInputBuf save() => this;

  import std.range;

  dchar front() {
    if (str.empty)
      return 0;
    return str.front;
  }

  void popFront() {
    if (!str.empty)
      str.popFront;
  }

  dchar popThenFront() {
    popFront;
    return front;
  }

  dchar frontThenPop() {
    scope (exit) popFront;
    return front;
  }
}

class JSParseState {
  const string buf_start;
  StrInputBuf buf_cur;

  JSToken token;
  JSFunctionDef fd;

  bool is_module;
  bool got_lf;

  void next_token() {
    import std.range;

    StrInputBuf p = buf_cur;
    got_lf = false;

    redo: token.pos = p;
    dchar c = p.front;

    switch (c) {
      case '\'', '\"':
        parse_string(c, true, p.dropOne, token, p);
        break;
      case '\n':
        p.popFront;
        got_lf = true;
        goto redo;
      case '\f', '\v', ' ', '\t':
        p.popFront;
        goto redo;
      default:
        token.val = c;
        p.popFront;
        break;
    }

    buf_cur = p;
  }

  void parse_string(
    int sep, bool do_throw, StrInputBuf p,
    ref JSToken token, ref StrInputBuf pp
  ) {
    import std.range;
    StrOutputBuf b;
    string p_escape;

    while (true) {
      if (buf_cur.str.empty)
        goto invalid_char;
      dchar c = p.front;
      if (c < 0x20) {
        if (sep == '`') {
          if (c == '\r') {
            if (p.dropOne.front == '\n')
              p.popFront;
            c = '\n';
          } else if (c == '\n' || c == '\r')
            goto invalid_char;
        }
        p.popFront;
        if (c == sep)
          break;
        if (c == '$' && p.front == '{' && sep == '`') {
          p.popFront;
          break;
        }
        if (c == '\\') {
          long diff = buf_start.length - buf_cur.str.length;
          long prev = diff - 1;
          p_escape = buf_start[prev..$];
          c = p.front;
          switch (c) {
            case '\0':
              if (buf_cur.str.empty)
                goto invalid_char;
              p.popFront;
              break;
            case '\'', '\"', '\\':
              p.popFront;
              break;
            case '\r':
              if (p.dropOne.front == '\n')
                p.popFront;
              goto case;
            case '\n':
              p.popFront;
              continue;
            case '0': .. case '9':
              if (!(fd.js_mode & JS_MODE_STRICT) && sep != '`')
                goto default;
              if (c == '0' && !(p.dropOne.front >= '0' && p.dropOne.front <= '9')) {
                p.popFront;
                c = '\0';
              } else {
                if (c >= '8' || sep == '`')
                  goto invalid_escape;
                else
                  assert(0);
              }
              break;
            case CP_LS, CP_PS:
              continue;
            default:
              invalid_escape: assert(0);
          }
        }
      }
    }
    invalid_char: assert(0);
  }
  
  void parse_directives() {
    import std.sumtype;
    if (!token.val.has!JSTokStr)
      return;

    assert(0);
  }

  void parse_program() {
    next_token;
    parse_directives;
    
    fd.is_global_var = (fd.eval_type == JS_EVAL_TYPE.GLOBAL) ||
      (fd.eval_type == JS_EVAL_TYPE.MODULE) ||
      !fd.is_struct;

    if (!is_module)
      fd.eval_ret = fd.add_var("<ret>");

    while (token.val != JSTokenVal(JS_TOK.EOF))
      parse_source_element;
  }

  void parse_source_element() {
    if (token.val == JSTokenVal(JS_TOK.FUNCTION))
      parse_function_decl(
        JS_PARSE_FUNC.STATEMENT, JS_FUNC_NORMAL,
        null, token.pos
      );
    else if (
      token_is_pseudo_keyword("async") &&
      peek_token(true) == JS_TOK.FUNCTION
    )
      parse_function_decl(
        JS_PARSE_FUNC.STATEMENT, JS_FUNC_NORMAL,
        null, token.pos
      );
  }

  void parse_function_decl(
    int func_type, int func_kind, JSAtom func_name,
    StrInputBuf pos, int export_flag, out JSFunctionDef pfd
  ) {
    bool is_expr = (
      func_type != JS_PARSE_FUNC.STATEMENT ||
      func_type != JS_PARSE_FUNC.VAR
    );

    if (
      func_type == JS_PARSE_FUNC.STATEMENT ||
      func_type == JS_PARSE_FUNC.VAR ||
      func_type == JS_PARSE_FUNC.EXPR
    ) {
      if (
        func_kind == JS_FUNC_NORMAL &&
        token_is_pseudo_keyword("async") &&
        peek_token(true) != '\n'
      ) {
        next_token;
        func_kind = JS_FUNC_ASYNC;
      }
      next_token;
      if (token.val == JSTokenVal('*')) {
        next_token;
        func_kind |= JS_FUNC_GENERATOR;
      }
    }

    assert(0);
  }

  void parse_function_decl(
    int func_type, int func_kind,
    JSAtom func_name, StrInputBuf pos
  ) {
    JSFunctionDef pfd;
    return parse_function_decl(
      func_type, func_kind, func_name, pos,
      JS_PARSE_EXPORT.NONE, pfd
    );
  }

  int peek_token(bool no_line_terminator) =>
    simple_next_token(buf_cur, no_line_terminator);

  bool token_is_pseudo_keyword(string keyword) {
    import std.sumtype;
    return token.val.has!JSTokIdent &&
      token.val.get!JSTokIdent.str == keyword &&
      token.val.get!JSTokIdent.has_escape == 0;
  }
}
