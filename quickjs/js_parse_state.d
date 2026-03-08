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
        token.val = JSTokDch(c);
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

    while (!token.val.has!JSTokEof)
      parse_source_element;
  }

  void parse_source_element() {
    if (token.val.has!JSTokFunction)
      return;
    else if (
      token_is_pseudo_keyword(ctx.rt.JS_ATOM_async) &&
      peek_token(true).has!JSTokFunction
    )
      return;
  }

  JSTokenVal peek_token(bool no_line_terminator) {
    assert(0);
  } 

  bool token_is_pseudo_keyword(JSAtom atom) =>
    token.val.has!JSTokIdent && token.val.get!JSTokIdent.atom is atom &&
    !token.val.get!JSTokIdent.has_escape;
}
