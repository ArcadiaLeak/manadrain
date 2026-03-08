module quickjs.js_token;
import quickjs;

import std.sumtype;

struct JSTokStr {
  JSValue str;
  int sep;
}

struct JSTokNum {
  JSValue num;
}

struct JSTokIdent {
  JSAtom atom;
  bool has_escape;
  bool is_reserved;
}

struct JSTokRegexp {
  JSValue body;
  JSValue flags;
}

struct JSTokDch {
  dchar ch;
}

struct JSTokEof {}

struct JSTokFunction {}

struct JSTokImport {}

struct JSTokExport {}

alias JSTokenVal =  SumType!(
  JSTokStr, JSTokNum, JSTokIdent, JSTokRegexp,
  JSTokDch, JSTokEof, JSTokFunction, JSTokImport,
  JSTokExport
);

struct JSToken {
  string pos;
  JSTokenVal val;
}
