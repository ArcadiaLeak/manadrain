module bison.gram;
import bison;

item_number[] ritem;
int nritems = 0;

rule[] rules;
rule_number nrules = rule_number(0);

symbol[] symbols;
int nsyms = 0;
int ntokens = 1;
int nnterms = 0;

symbol_number[] token_translations;
int max_code;

struct item_number {
  int _;
  alias _ this;
}

struct item_index {
  size_t _;
  alias _ this;
}

struct rule_number {
  int _;
  alias _ this;
}

struct rule {
  rule_number number;

  sym_content lhs;
  item_number[] rhs;

  bool useful;
}

void item_print(item_number[] item) {
  const ref rule r = item_rule(item);
  r.rule_lhs_print;

  import std.stdio;
  if (r.rhs[0] >= 0) {
    foreach (sym; r.rhs[0..r.rhs.length - item.length])
      writef(" %s", symbols[sym].tag);
    writef(" %s", '\u2022');
    foreach (sym; item)
      if (sym >= 0) writef(" %s", symbols[sym._].tag);
      else break;
  } else
    writef(" %s %s", '\u03B5', '\u2022');
}

ref rule item_rule(item_number[] item) {
  item_number[] sp = item;
  while (sp[0] >= 0)
    sp = sp[1..$];
  rule_number r = rule_number(-1 - sp[0]);
  return rules[r];
}

void rule_lhs_print(in rule r) {
  import std.stdio;
  writef("  %3d ", r.number);
  writef("%s:", r.lhs.symbol_.tag);
}

void rule_rhs_print(in rule r) {
  import std.stdio;
  if (r.rhs[0] >= 0)
    for (int k = 0; r.rhs[k] >= 0; k++)
      writef(" %s", symbols[r.rhs[k]].tag);
  else
    writef(" %s", '\u03B5');
}

size_t ritem_longest_rhs() {
  size_t max = 0;
  foreach (r; 0..nrules) {
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
