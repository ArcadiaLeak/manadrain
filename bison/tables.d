module bison.tables;
import bison;

void tables_generate(
  int ntokens,
  int nnterms,
  int nsyms,
  int nstates
) {
  const int nvectors = nstates + nnterms;

  int[][] froms = new int[][nvectors];
  int[][] tos = new int[][nvectors];
  int[][] conflict_tos = new int[][nvectors];
  size_t[] tally = new size_t[nvectors];
  int[] width = new int[nvectors];
  int[] order = new int[nvectors];
  int nentries;

  int[] base;
  int base_ninf = 0;
  bool[] pos_set;
  int pos_set_base = 0;
  enum int table_size = 32768;
  int[] table;
  int[] check;
  int table_ninf = 0;
  int[] conflict_table;

  int lowzero;
  int high;

  void sort_actions() {
    nentries = 0;

    foreach (i; 0..nvectors)
      if (tally[i] > 0) {
        size_t t = tally[i];
        int w = width[i];
        int j = nentries - 1;

        while (j >= 0 && width[order[j]] < w)
          j--;

        while (j >= 0 && width[order[j]] == w && tally[order[j]] < t)
          j--;

        for (int k = nentries - 1; k > j; k--)
          order[k + 1] = order[k];
        
        order[j + 1] = i;
        nentries++;
      }
  }

  int matching_state(int vector) {
    int i = order[vector];

    if (i < nstates) {
      size_t t = tally[i];
      int w = width[i];

      if (conflict_tos[i] !is null)
        foreach (j; 0..t)
          if (conflict_tos[i][j] != 0)
            return -1;
      
      foreach_reverse (prev; 0..vector) {
        int j = order[prev];
        if (width[j] != w || tally[j] != t)
          return -1;
        else {
          bool match = true;
          foreach (k; 0..t) {
            if (!match) break;
            if (tos[j][k] != tos[i][k] ||
              froms[j][k] != froms[i][k] ||
              (conflict_tos[j] !is null && conflict_tos[j][k] != 0))
              match = false;
          }
          if (match) return j;
        }
      }
    }

    return -1;
  }

  bool pos_set_test(int pos) {
    int bitno = pos - pos_set_base;
    return pos_set[bitno];
  }

  void pos_set_set(int pos) {
    int bitno = pos - pos_set_base;
    pos_set[bitno] = true;
  }

  int pack_vector(int vector) {
    int i = order[vector];
    size_t t = tally[i];
    int[] from = froms[i];
    int[] to = tos[i];
    int[] conflict_to = conflict_tos[i];

    import std.range;
    foreach (res; recurrence!"a[n-1] + 1"(lowzero - from[0])) {
      bool ok = true;
      {
        foreach (k; 0..t) {
          if (!ok) break;
          
          int loc = res + from[k];
          if (table[loc] != 0)
            ok = false;
        }

        if (ok && pos_set_test(res))
          ok = false;
      }
      
      if (ok) {
        int loc = -1;
        foreach (k; 0..t) {
          loc = res + from[k];
          table[loc] = to[k];
          check[loc] = from[k];
        }

        while (table[lowzero] != 0)
          lowzero++;
        
        if (high < loc)
          high = loc;
        
        return res;
      }
    }

    assert(0);
  }

  int table_ninf_remap(int[] tab) {
    int res;

    foreach (t; tab)
      if (t < res && t != int.min)
        res = t;

    --res;

    foreach (i; 0..tab.length)
      if (tab[i] == int.min)
        tab[i] = res;

    return res;
  }

  void pack_table() {
    base = new int[nvectors];
    pos_set = new bool[table_size + nstates];
    pos_set_base = -nstates;
    table = new int[table_size];
    conflict_table = new int[table_size];
    check = new int[table_size];

    lowzero = 0;
    high = 0;

    base[] = int.min;
    check[] = -1;

    foreach (i; 0..nentries) {
      int s = matching_state(i);
      int place;

      if (s < 0)
        place = pack_vector(i);
      else
        place = base[s];

      pos_set_set(place);
      base[order[i]] = place;
    }

    base_ninf = table_ninf_remap(base);
    table_ninf = table_ninf_remap(table[0..high + 1]);
  }

  token_actions;
  goto_actions;

  sort_actions;
  pack_table;
}
