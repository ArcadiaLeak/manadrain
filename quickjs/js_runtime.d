module quickjs.js_runtime;
import quickjs;

import core.stdc.stdint;
import core.stdc.stdlib;

enum JS_ATOM_TYPE {
  STRING = 1,
  GLOBAL_SYMBOL,
  SYMBOL,
  PRIVATE
}

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

class JSAtom {
  JS_ATOM_TYPE atom_type;
  uintptr_t uid;
  string str;

  this() { uid = cast(uintptr_t) malloc(1); }
  ~this() { free(cast(void*) uid); }
}

class JSRuntime {
  JSAtom[string] str_hash;
  void[][uintptr_t] atom_hash;

  void insert_wellknown() {
    for (int i = JS_ATOM_null; i < JS_ATOM_END; i++) {
      JS_ATOM_TYPE atom_type;
      if (i == JS_ATOM_Private_brand)
        atom_type = JS_ATOM_TYPE.PRIVATE;
      else if (i >= JS_ATOM_Symbol_toPrimitive)
        atom_type = JS_ATOM_TYPE.SYMBOL;
      else
        atom_type = JS_ATOM_TYPE.STRING;
      
      if (atom_type == JS_ATOM_TYPE.STRING) {
        string str = js_atom_init[i - 1];
        JSAtom str_atom = new JSAtom;
        str_atom.str = str;
        str_hash[str] = str_atom;
        atom_hash[str_atom.uid] = null;
      }
    }
  }
}
