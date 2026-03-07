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
  string buf;
  
  JSToken token;
  JSFunctionDef cur_func;

  bool is_module;

  int next_token() {
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
    return 0;
  }

  int parse_directives() {
    if (!token.val.has!JSTokStr)
      return 0;

    assert(0);
  }

  int parse_program() {
    if (next_token)
      return -1;

    if (parse_directives)
      return -1;
    
    cur_func.is_global_var = (cur_func.eval_type == JS_EVAL_TYPE.GLOBAL) ||
      (cur_func.eval_type == JS_EVAL_TYPE.MODULE) ||
      !cur_func.is_struct;

    if (!is_module) {
      assert(0);
    }

    assert(0);
  }
}
