module bison.closure;
import bison;

import std.range.interfaces;

auto closure_new(
  const int nritems,
  const int nrules,
  const int ntokens,
  const int nnterms,
  const int nsyms,
  const int[] ritem,
  const rule[] rules,
  const rule[][][] derives,
  const symbol[] symbols
) {
  size_t[] itemset = new size_t[nritems];
  size_t nitemset;

  bool[] ruleset = new bool[nrules];

  bool[][] fderives;
  bool[][] firsts;

  void print_firsts() {
    import std.stdio;

    write("FIRSTS\n");

    foreach (i; ntokens..nsyms) {
      writef("  %s firsts\n", symbols[i].tag);
      foreach (j, flag; firsts[i - ntokens])
        if (flag) writef("    %s\n", symbols[j + ntokens].tag);
    }

    write("\n\n");
  }

  void print_fderives() {
    import std.stdio;

    write("FDERIVES\n");

    foreach (i; ntokens..nsyms) {
      writef("  %s derives\n", symbols[i].tag);
      foreach (r, flag; fderives[i - ntokens])
        if (flag) {
          writef("    %3d ", r);
          symbols.rule_rhs_print(rules[r]);
          write("\n");
        }
    }

    write("\n\n");
  }

  void set_firsts() {
    import std.array;
    import std.range;

    bool[] buffer = new bool[nnterms * nnterms];
    firsts = buffer.chunks(nnterms).array;
    
    foreach (i; ntokens..nsyms)
      for (int j = 0; derives[i - ntokens][j]; ++j) {
        int sym = derives[i - ntokens][j].front.rhs.front;
        if(sym >= ntokens)
          firsts[i - ntokens][sym - ntokens] = true;
      }
    
    foreach (i; 0..nnterms)
      foreach (j; 0..nnterms)
        if (firsts[j][i])
          firsts[j][] |= firsts[i][];

    foreach (i; 0..nnterms)
      firsts[i][i] = true;

    if (TRACE_SETS)
      print_firsts;
  }

  void set_fderives() {
    import std.array;
    import std.range;

    bool[] buffer = new bool[nnterms * nrules];
    fderives = buffer.chunks(nrules).array;

    set_firsts;

    foreach (i; ntokens..nsyms)
      foreach (j; ntokens..nsyms)
        if (firsts[i - ntokens][j - ntokens])
          for (int k = 0; derives[j - ntokens][k]; ++k)
            fderives[i - ntokens][derives[j - ntokens][k].front.number] = true;

    if (TRACE_SETS)
      print_fderives;
  }

  void closure_print(string title, const size_t[] arr) {
    import std.stdio;

    writef("Closure: %s\n", title);

    foreach (i; arr) {
      writef("  %2d: .", i);
      const(int[]) rp = ritem[i..$];
      size_t r;
      for (r = 0; rp[r] >= 0; r++)
        writef(" %s", symbols[rp[r]].tag);
      writef("  (rule %d)\n", -1 - rp[r]);
    }

    write("\n\n");
  }

  void closure(const state s) {
    if (TRACE_CLOSURE)
      closure_print("input", s.items);

    ruleset[] = 0;

    foreach (c; s.items)
      if (ritem[c] >= ntokens)
        ruleset[] |= fderives[ritem[c] - ntokens][];

    nitemset = 0;
    size_t c = 0;

    foreach (ruleno, flag; ruleset)
      if (flag) {
        size_t itemno = ritem.length - rules[ruleno].rhs.length;
        while (c < s.items.length && s.items[c] < itemno) {
          itemset[nitemset] = s.items[c];
          nitemset++;
          c++;
        }
        itemset[nitemset] = itemno;
        nitemset++;
      }

    while (c < s.items.length) {
      itemset[nitemset] = s.items[c];
      nitemset++;
      c++;
    }

    if (TRACE_CLOSURE)
      closure_print("output", itemset[0..nitemset]);
  }

  set_fderives;

  return &closure;
}
