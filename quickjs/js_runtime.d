module quickjs.js_runtime;
import quickjs;

class JSRuntime {
  JSStrTree str_tree;
  JSAtom atom_list;

  const void[][JSAtom] is_const_atom;

  static foreach (catom; JS_ATOM)
    mixin("JSString JS_ATOM_", catom[0], ";");

  this() {
    static foreach (catom; JS_ATOM) {
      mixin("JS_ATOM_", catom[0], " = new JSString(\"", catom[1],"\");");
      is_const_atom[mixin("JS_ATOM_", catom[0])] = null;
    }
  }
}
