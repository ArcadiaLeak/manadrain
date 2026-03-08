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
    void[][JSAtom] const_atom_aa;
    static foreach (i, atom; JS_ATOM) {
      mixin("JS_ATOM_", atom[1], " = new ", atom[0], "(\"", atom[2], "\");");
      const_atom_aa[mixin("JS_ATOM_", atom[1])] = null;
      static if (i == 0) {
        atom_list_begin = mixin("JS_ATOM_", atom[1]);
        atom_list_end = mixin("JS_ATOM_", atom[1]);
      } else {
        mixin("JS_ATOM_", atom[1]).prev = atom_list_end;
        atom_list_end.next = mixin("JS_ATOM_", atom[1]);
        atom_list_end = mixin("JS_ATOM_", atom[1]);
      }
    }
    is_const_atom = const_atom_aa;
  }
}
