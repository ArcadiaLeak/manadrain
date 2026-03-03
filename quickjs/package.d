module quickjs;

import std.container.dlist;

enum JSGCPhase {
  NONE,
  DECREF,
  REMOVE_CYCLES
}

class JSRuntime {
  DList!size_t context_list;
  DList!size_t gc_obj_list;
  DList!size_t gc_zero_ref_count_list;
  DList!size_t tmp_obj_list;
  size_t malloc_gc_threshold = 256 * 1024;
  DList!size_t weakref_list;

  int InitAtoms() {
    assert(0);
  }
}

void main(string[] args) {
  
}
