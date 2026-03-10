module quickjs.js_runtime;
import quickjs;

enum JS_ATOM_TYPE {
  STRING = 1,
  GLOBAL_SYMBOL,
  SYMBOL,
  PRIVATE
}

abstract class JSObject {
  uint ref_count = 1;
  JSAtom as_atom() => null;
}

class JSAtom : JSObject {
  int atom_type;
  string str;
  override JSAtom as_atom() => this;
}

class JSRuntime {
  uint generation;
  JSObject[uint] by_handle;
  uint[JSObject] by_ptr;

  uint[] atom_array;
  uint[string] atom_hash;
  int atom_free_index;

  uint admit(JSObject obj) {
    uint handle = generation++;
    by_handle[handle] = obj;
    by_ptr[obj] = handle;
    return handle;
  }

  JSObject expel(uint handle) {
    JSObject obj = by_handle[handle];
    by_handle.remove(handle);
    by_ptr.remove(obj);
    return obj;
  }

  uint new_atom(string str, int atom_type) {
    JSAtom p = new JSAtom;
    p.str = str;
    return new_atom(admit(p), atom_type);
  }

  uint new_atom(uint str_handle, int atom_type) {
    JSAtom str = by_handle[str_handle].as_atom;
    uint i;

    if (atom_type < JS_ATOM_TYPE.SYMBOL) {
      if (str.atom_type == atom_type) {
        
      }
    }

    assert(0);
  }
}
