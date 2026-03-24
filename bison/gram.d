module bison.gram;
import bison;

struct rule {
  int number;

  sym_content lhs;
  int[] rhs;

  bool useful;
}

void rule_lhs_print(in rule r) {
  import std.stdio;
  writef("  %3d ", r.number);
  writef("%s:", r.lhs.symbol_.tag);
}

void rule_rhs_print(const symbol[] symbols, const rule r) {
  import std.stdio;

  if (r.rhs[0] >= 0)
    for (int k = 0; r.rhs[k] >= 0; k++)
      writef(" %s", symbols[r.rhs[k]].tag);
  else
    writef(" %s", '\u03B5');
}

void item_print(symbol[] symbols, rule[] rules, int[] item) {
  const ref rule r = rules.item_rule(item);
  r.rule_lhs_print;

  import std.stdio;
  if (r.rhs[0] >= 0) {
    foreach (sym; r.rhs[0..r.rhs.length - item.length])
      writef(" %s", symbols[sym].tag);
    writef(" %s", '\u2022');
    foreach (sym; item)
      if (sym >= 0) writef(" %s", symbols[sym].tag);
      else break;
  } else
    writef(" %s %s", '\u03B5', '\u2022');
}

ref rule item_rule(rule[] rules, int[] item) {
  int[] sp = item;
  while (sp[0] >= 0)
    sp = sp[1..$];
  int r = -1 - sp[0];
  return rules[r];
}

size_t ritem_longest_rhs(rule[] rules) {
  size_t max = 0;
  foreach (r; 0..rules.length) {
    size_t length = rule_rhs_length(rules[r..$]);
    if (length > max)
      max = length;
  }

  return max;
}

size_t rule_rhs_length(const rule[] r) {
  size_t res = 0;
  for (size_t i = 0; r[0].rhs[i] >= 0; ++i)
    ++res;
  return res;
}
