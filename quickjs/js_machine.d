module quickjs.js_machine;
import quickjs;

import std.sumtype;

alias JSValue = SumType!(
  int, long, double, Object
);

enum JS_EVAL_TYPE {
  GLOBAL, MODULE, DIRECT, INDIRECT
}

class JSVarDef {
  JSAtom var_name;
  Object func_pool;

  JSVarDef prev;
  JSVarDef next;
}

class JSVarScope {
  JSVarScope parent;
  JSVarDef first;

  JSVarScope prev;
  JSVarScope next;
}

class JSMachineOp {
  JSMachineOp next;
  JSMachineOp prev;
}

class JSEnterScope : JSMachineOp {
  JSVarScope scope_;
}
