module quickjs.js_runtime;
import quickjs;

class JSRuntime {
  JSStrTree str_tree;
  JSAtom atom_list_begin;
  JSAtom atom_list_end;

  const void[][JSAtom] is_const_atom;

  static foreach (catom; JS_ATOM)
    mixin(catom[0], " JS_ATOM_", catom[1], ";");

  this() {
    static foreach (i, catom; JS_ATOM) {
      mixin("JS_ATOM_", catom[1], " = new ", catom[0], "(\"", catom[2], "\");");
      static if (i == 0) {
        atom_list_begin = mixin("JS_ATOM_", catom[1]);
        atom_list_end = mixin("JS_ATOM_", catom[1]);
      } else {
        mixin("JS_ATOM_", catom[1]).prev = atom_list_end;
        atom_list_end.next = mixin("JS_ATOM_", catom[1]);
        atom_list_end = mixin("JS_ATOM_", catom[1]);
      }
      is_const_atom[mixin("JS_ATOM_", catom[1])] = null;
    }
  }
}
