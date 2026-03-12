module quickjs.js_machine;
import quickjs;

class JSMchInst {
  JSMchInst next;
  JSMchInst prev;
}

class JSEnterScope : JSMchInst {
  JSVarScope scope_;
}
