module quickjs.js_atom_clazz;
import quickjs;

class JSAtomClazz {
  JSAtomTree.Color color;
  JSAtomClazz left;
  JSAtomClazz right;
  JSAtomClazz parent;

  abstract uint key();
}

class JSString : JSAtomClazz {
  uint hash;
  override uint key() => hash;

  abstract JSString8 str8();
  abstract JSString16 str16();
}

class JSString8 : JSString {
  override JSString8 str8() => this;
  override JSString16 str16() => null;
}

class JSString16 : JSString {
  override JSString8 str8() => null;
  override JSString16 str16() => this;
}
