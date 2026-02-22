module bison.gram;
import bison;

struct rule {
  int number;

  sym_content lhs;
  int[] rhs;

  bool useful;
}