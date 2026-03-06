module quickjs.js_atom_bhvi;
import quickjs;

immutable class JS_ATOM {
  string str;
  int atom_type;

  this(string s, int at) {
    str = s;
    atom_type = at;
  }
}

void JS_ResizeAtomHash(JSRuntime rt, int new_hash_size) {
  
}
