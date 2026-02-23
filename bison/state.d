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
