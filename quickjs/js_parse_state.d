module quickjs.js_parse_state;
import quickjs;

import std.sumtype;

alias JSValue = SumType!(
  int, long, double, Object
);

struct JSTokStr {
  JSValue str;
  int sep;
}

struct JSTokNum {
  JSValue num;
}

struct JSTokIdent {
  Object atom;
  bool has_escape;
  bool is_reserved;
}

struct JSTokRegexp {
  JSValue body;
  JSValue flags;
}

struct JSTokDch {
  dchar ch;
}

struct JSToken {
  string pos;
  SumType!(
    JSTokStr, JSTokNum, JSTokIdent, JSTokRegexp, JSTokDch
  ) val;
}

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

  int parse_program() {
    next_token;
    parse_directives;
    
    fd.is_global_var = (fd.eval_type == JS_EVAL_TYPE.GLOBAL) ||
      (fd.eval_type == JS_EVAL_TYPE.MODULE) ||
      !fd.is_struct;

    if (!is_module)
      fd.eval_ret = fd.add_var(ctx.rt.JS_ATOM__ret_);

    assert(0);
  }
}
