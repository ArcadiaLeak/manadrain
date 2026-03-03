module duktape.duk_api_heap;
import duktape;

duk_hthread* duk_create_heap() {
  return duk_heap_alloc.heap_thread;
}
