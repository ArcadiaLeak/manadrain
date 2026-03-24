module harfbuzz.hb_atomic;

import core.atomic;

struct hb_atomic_t(T) {
  this(T in_v) { v = in_v; }
  this(const ref hb_atomic_t o) { v = o.get_relaxed; }
  this(hb_atomic_t o) { v = o.get_relaxed; o.set_relaxed = T.init; }

  ref hb_atomic_t opAssign(const ref hb_atomic_t o) {
    set_relaxed(o.get_relaxed); return this; }
  ref hb_atomic_t opAssign(hb_atomic_t o) {
    set_relaxed(o.get_relaxed); o.set_relaxed = T.init; return this; }
  ref hb_atomic_t opAssign(T in_v) {
    set_relaxed(in_v); return this; }

  alias this = get_relaxed;

  void set_relaxed(T in_v) { v.atomicStore!(MemoryOrder.raw) = in_v; }
  void set_release(T in_v) { v.atomicStore!(MemoryOrder.rel) = in_v; }
  T get_relaxed() const => v.atomicLoad!(MemoryOrder.raw);
  T get_acquire() const => v.atomicLoad!(MemoryOrder.acq);
  T inc() => v.atomicFetchAdd!(MemoryOrder.acq_rel)(1);
  T dec() => v.atomicFetchAdd!(MemoryOrder.acq_rel)(-1);

  int opUnary(string s : "++")() => cast(int) inc;
  int opUnary(string s : "--")() => cast(int) dec;

  shared(T) v = 0;
}

struct hb_atomic_t(T : T*) {
  this(immutable(T)* in_v) { v = in_v; }
  @disable this(ref inout(hb_atomic_t) o);

  void init(immutable(T)* in_v = null) { set_relaxed(in_v); }
  void set_relaxed(immutable(T)* in_v) { v.atomicStore!(MemoryOrder.raw) = in_v; }
  immutable(T)* get_relaxed() const => v.atomicLoad!(MemoryOrder.raw);
  immutable(T)* get_acquire() => v.atomicLoad!(MemoryOrder.acq);
  bool cmpexch(immutable(T)* old, immutable(T)* new_) =>
    casWeak!(MemoryOrder.acq_rel, MemoryOrder.raw)(&v, old, new_);

  alias this = get_acquire;

  immutable(T)* v = null;
}
