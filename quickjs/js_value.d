module quickjs.js_value;
import quickjs;

import core.stdc.stdint;

union JSValueUnion {
  int int32;
  double float64;
  uintptr_t ptr;
  long short_big_int;
}

struct JSValue {
  JSValueUnion u;
  long tag;
}
