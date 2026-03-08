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

class JSParseState {
  JSContext ctx;
  string buf;
  
  JSToken token;
  JSFunctionDef fd;

  bool is_module;

  void next_token() {
    import std.range.primitives;

    string p = buf;
    token.pos = p;

    dchar c = p.front;
    switch (c) {
      default:
        token.val = c;
        p.popFront;
        break;
    }
    buf = p;
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
      fd.eval_ret = fd.add_var(ctx.rt.JS_ATOM__ret_);

    while (token.val != JSTokenVal(JS_TOK.EOF))
      parse_source_element;
  }

  void parse_source_element() {
    if (token.val == JSTokenVal(JS_TOK.FUNCTION))
      parse_function_decl(JS_PARSE_FUNC.STATEMENT, JS_FUNC_NORMAL, null, token.pos);
    else if (
      token_is_pseudo_keyword(ctx.rt.JS_ATOM_async) &&
      peek_token(true) == JSTokenVal(JS_TOK.FUNCTION)
    )
      parse_function_decl(JS_PARSE_FUNC.STATEMENT, JS_FUNC_NORMAL, null, token.pos);
  }

  void parse_function_decl(
    int func_type, int func_kind, JSAtom func_name,
    string pos, int export_flag, out JSFunctionDef pfd
  ) {
    assert(0);
  }

  void parse_function_decl(
    int func_type, int func_kind,
    JSAtom func_name, string pos
  ) {
    JSFunctionDef pfd;
    return parse_function_decl(
      func_type, func_kind, func_name, pos,
      JS_PARSE_EXPORT.NONE, pfd
    );
  }

  JSTokenVal peek_token(bool no_line_terminator) =>
    simple_next_token(buf, no_line_terminator);

  bool token_is_pseudo_keyword(JSAtom atom) {
    import std.sumtype;
    return token.val.has!JSTokIdent &&
      token.val.get!JSTokIdent.atom is atom &&
      token.val.get!JSTokIdent.has_escape == 0;
  }
}
