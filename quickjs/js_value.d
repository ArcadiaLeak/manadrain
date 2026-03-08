module quickjs.js_value;
import quickjs;

import std.sumtype;

alias JSValue = SumType!(
  int, long, double, Object
);
