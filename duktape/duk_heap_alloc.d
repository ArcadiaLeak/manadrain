module duktape.duk_heap_alloc;
import duktape;

struct duk_heap {
  uint ms_prevent_count;
  uint pf_prevent_count;

  duk_hthread* heap_thread;

  int call_recursion_depth;
  int call_recursion_limit;
}

duk_heap* duk_heap_alloc() {
  duk_heap* res = new duk_heap;

  res.ms_prevent_count = 1;
  res.pf_prevent_count = 1;

  res.call_recursion_depth = 0;
  res.call_recursion_limit = DUK_USE_NATIVE_CALL_RECLIMIT;

  return res;
}
