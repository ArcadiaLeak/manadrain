module quickjs.js_atom;
import quickjs;

import core.atomic;
import std.sumtype;

shared ulong symbol_id;

struct JSString {
  string str;
}

struct JSSymbol {
  const ulong id;
  string desc;

  @disable this();
  this(string desc) {
    id = symbol_id.atomicFetchAdd(1);
  }
}

alias JSAtom = SumType!(JSString, JSSymbol);
