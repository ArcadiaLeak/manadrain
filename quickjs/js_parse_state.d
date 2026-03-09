module quickjs.js_parse_state;
import quickjs;

enum JS_PARSE_FUNC {
  STATEMENT, VAR, EXPR, ARROW, GETTER, SETTER, METHOD,
  CLASS_STATIC_INIT, CLASS_CONSTRUCTOR, DERIVED_CLASS_CONSTRUCTOR
}

enum JS_PARSE_EXPORT {
  NONE, NAMED, DEFAULT
}

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
  StrInputBuf buf;

  JSToken token;
  JSFunctionDef fd;

  bool is_module;
  bool got_lf;

  void next_token() {
    import std.range;

    StrInputBuf p = buf;
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

    buf = p;
  }

  void parse_string(
    int sep, bool do_throw, StrInputBuf p,
    ref JSToken token, ref StrInputBuf pp
  ) {
    import std.range;
    StrOutputBuf b;

    while (true) {
      dchar c = p.front;
      if (c < 0x20) {
        if (sep == '`') {
          if (c == '\r') {
            if (p.dropOne.front == '\n')
              p.popFront;
            c = '\n';
          }
        }
      }
    }
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
        JSAtom(JSString(null)), token.pos
      );
    else if (
      token_is_pseudo_keyword("async") &&
      peek_token(true) == JS_TOK.FUNCTION
    )
      parse_function_decl(
        JS_PARSE_FUNC.STATEMENT, JS_FUNC_NORMAL,
        JSAtom(JSString(null)), token.pos
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
    simple_next_token(buf, no_line_terminator);

  bool token_is_pseudo_keyword(string keyword) {
    import std.sumtype;
    return token.val.has!JSTokIdent &&
      token.val.get!JSTokIdent.str == keyword &&
      token.val.get!JSTokIdent.has_escape == 0;
  }
}
