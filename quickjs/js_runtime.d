module quickjs.js_runtime;
import quickjs;

import std.container.rbtree;

class JSRuntime {
  RedBlackTree!(
    JSString, "a.str < b.str"
  ) str_hash;
}
