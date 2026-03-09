module quickjs.js_machine;
import quickjs;

enum JS_EVAL_TYPE {
  GLOBAL, MODULE, DIRECT, INDIRECT
}

class JSVarDef {
  string var_name;
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

class JSMchInst {
  JSMchInst next;
  JSMchInst prev;
}

class JSEnterScope : JSMchInst {
  JSVarScope scope_;
}
