module bison.gram;
import bison;

struct rule {
  int number;

  sym_content lhs;
  int[] rhs;

  bool useful;
}

void rule_rhs_print(const symbol[] symbols, const rule r) {
  import std.stdio;

  if (r.rhs[0] >= 0)
    for (int k = 0; r.rhs[k] >= 0; k++)
      writef(" %s", symbols[r.rhs[k]].tag);
  else
    writef(" %s", '\u03B5');
}
