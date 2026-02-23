module bison.state;
import bison;

class state {
  int accessing_symbol;
  immutable size_t[] items;
  rule[][] reductions;
  state[] transitions;
  int number;
  bool consistent;
  bool[][] lookaheads;
  symbol[] errs;

  this(int sym, immutable size_t[] core) {
    accessing_symbol = sym;
    items = core;
  }
}

void state_transitions_print(symbol[] symbols, state s) {
  import std.stdio;
  writef("transitions of %d (%d):\n", s.number, s.transitions.length);
  foreach (i, trans; s.transitions)
    writef(
      "  %d: (%d, %s, %d)\n",
      i,
      s.number,
      symbols[trans.accessing_symbol].tag,
      trans.number
    );
}

state transitions_to(state s, int sym) {
  foreach (trans; s.transitions)
    if (trans.accessing_symbol == sym)
      return trans;
  assert(0);
}

int state_reduction_find(state s, rule[] r) {
  rule[][] reds = s.reductions;
  foreach (i; 0..reds.length)
    if (reds[i] == r)
      return cast(int) i;
  assert(0);
}
