module bison.symlist;
import bison;

class symbol_list {
  symbol sym;
  symbol_list next;

  this(symbol sym) {
    this.sym = sym;
  }
}
