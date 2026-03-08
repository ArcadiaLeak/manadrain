module quickjs.js_function_def;
import quickjs;

class JSFunctionDef {
  JSMachineOp machine_op;

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
    if (machine_op) {
      enter_scope.prev = machine_op;
      enter_scope.next = machine_op.next;
      machine_op.next = enter_scope;
    }
    machine_op = enter_scope;

    scope_level = new_scope;
  }
}

