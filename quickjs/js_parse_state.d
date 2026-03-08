module quickjs.js_parse_state;
import quickjs;

import std.sumtype;

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
      return;
    else if (
      token_is_pseudo_keyword(ctx.rt.JS_ATOM_async) &&
      peek_token(true) == JSTokenVal(JS_TOK.FUNCTION)
    )
      return;
  }

  JSTokenVal peek_token(bool no_line_terminator) =>
    simple_next_token(buf, no_line_terminator);

  bool token_is_pseudo_keyword(JSAtom atom) =>
    token.val.has!JSTokIdent && token.val.get!JSTokIdent.atom is atom &&
    !token.val.get!JSTokIdent.has_escape;
}
