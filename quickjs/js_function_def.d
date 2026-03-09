module quickjs.js_function_def;
import quickjs;

class JSFunctionDef {
  JSMchInst code;

  JSVarDef vars_begin;
  JSVarDef vars_end;
  JSVarDef eval_ret;

  JSVarScope scope_level;
  JSVarDef scope_first;

  JS_EVAL_TYPE eval_type;
  bool is_struct;
  bool is_global_var;

  void push_scope() {
    JSVarScope new_scope = new JSVarScope;
    new_scope.parent = scope_level;
    new_scope.first = scope_first;

    JSEnterScope enter_scope = new JSEnterScope;
    enter_scope.scope_ = new_scope;
    if (code) {
      enter_scope.prev = code;
      enter_scope.next = code.next;
      code.next = enter_scope;
    }
    code = enter_scope;

    scope_level = new_scope;
  }

  JSVarDef add_var(string name) {
    JSVarDef vd = new JSVarDef;
    vd.var_name = name;

    if (vars_begin is null)
      vars_begin = vd;
    if (vars_end is null)
      vars_end = vars_begin;
    else {
      vd.prev = vars_end;
      vars_end.next = vd;
      vars_end = vd;
    }

    return vd;
  }
}
