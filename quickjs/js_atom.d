module quickjs.js_atom;
import quickjs;

class JSAtom {
  JSAtom prev;
  JSAtom next;
  JSString as_string() => null;
}

class JSString : JSAtom {
  wstring str;
  this(wstring s) { str = s; }
  override JSString as_string() => this;
}