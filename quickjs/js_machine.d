module quickjs.js_machine;
import quickjs;

enum JS_EVAL_TYPE {
  GLOBAL, MODULE, DIRECT, INDIRECT
}

struct JSVarDef {
  long iatom;
  long ifuncpool;
}

class JSVarScope {
  JSVarScope parent;
  JSVarDef first;
}

class JSMachineOp {
  JSMachineOp next;
  JSMachineOp prev;
}

class JSEnterScope : JSMachineOp {
  JSVarScope scope_;
}
