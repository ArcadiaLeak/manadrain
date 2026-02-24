module bison.tables;
import bison;

import std.typecons;

auto tables_generate(
  int ntokens,
  int nnterms,
  int nsyms,
  int nstates,
  state[] states,
  rule[] rules,
  symbol errtoken,
  int[] goto_map,
  int[] from_state,
  int[] to_state,
  symbol[] symbols
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
  int[] actrow;
  int[] conflrow;
  int[] conflict_table;
  int[] conflict_list;
  int conflict_list_cnt;
  int conflict_list_free;
  int lowzero;
  int high;
  int[] yydefgoto;
  int[] yydefact;

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

  rule[] action_row(state s) {
    import std.range;

    foreach (i; 0..ntokens)
      actrow[i] = conflrow[i] = 0;
    
    bool conflicted = false;
    if (s.lookaheads)
      foreach_reverse (la, r; zip(s.lookaheads, s.reductions))
        foreach (sym, flag; la)
          if (flag) {
            if (actrow[sym] != 0) {
              conflicted = true;
              conflrow[sym] = 1;
            }
            actrow[sym] = -1 - r[0].number;
          }

    bool nodefault = false;
    foreach (shift_state; s.transitions) {
      if (shift_state is null)
        continue;
      if (shift_state.accessing_symbol >= ntokens)
        break;

      int sym = shift_state.accessing_symbol;
      
      if (actrow[sym] != 0) {
        conflicted = true;
        conflrow[sym] = 1;
      }
      actrow[sym] = shift_state.number;

      if (sym == errtoken.content.number)
        nodefault = true;
    }

    rule[] default_reduction = null;
    if (s.reductions.length >= 1 && !nodefault) {
      if (s.consistent)
        default_reduction = s.reductions[0];
      else {
        int max = 0;
        foreach (r; s.reductions) {
          int count = 0;
          foreach (sym; 0..ntokens)
            if (actrow[sym] == -1 - r[0].number)
              count++;
          
          if (count > max) {
            max = count;
            default_reduction = r;
          }
        }

        if (max > 0)
          foreach (sym; 0..ntokens)
            if (actrow[sym] == -1 - default_reduction[0].number)
              actrow[sym] = 0;
      }
    }

    if (!default_reduction)
      foreach (i; 0..ntokens)
        if (actrow[i] == int.min)
          actrow[i] = 0;

    return default_reduction;
  }

  void save_row(size_t s) {
    size_t count = 0;
    foreach (i; 0..ntokens)
      if (actrow[i] != 0)
        count++;
    
    if (count) {
      int[] sp1 = froms[s] = new int[count];
      int[] sp2 = tos[s] = new int[count];
      int[] sp3 = conflict_tos[s] = null;

      foreach (i; 0..ntokens)
        if (actrow[i] != 0) {
          sp1[0] = i;
          sp1 = sp1[1..$];
          sp2[0] = actrow[i];
          sp2 = sp2[1..$];
        }

      tally[s] = count;
      width[s] = froms[s][froms[s].length - sp1.length - 1] - froms[s][0] + 1;
    }
  }

  void token_actions() {
    int nconflict = 0;

    yydefact = new int[nstates];

    actrow = new int[ntokens];
    conflrow = new int[ntokens];

    conflict_list = new int[1 + 2 * nconflict];
    conflict_list_free = 2 * nconflict;
    conflict_list_cnt = 1;

    foreach (r; rules)
      r.useful = false;

    foreach (i; 0..nstates) {
      rule[] default_reduction = action_row(states[i]);
      yydefact[i] = default_reduction ? default_reduction[0].number + 1 : 0;
      save_row(i);

      foreach (j; 0..ntokens)
        if (actrow[j] < 0 && actrow[j] != int.min)
          rules[-1 - actrow[j]].useful = true;
      if (yydefact[i])
        rules[yydefact[i] - 1].useful = true;
    }
  }

  void save_column(int sym, int default_state) {
    int begin = goto_map[sym - ntokens];
    int end = goto_map[sym - ntokens + 1];

    size_t count = 0;
    foreach (i; begin..end)
      if (to_state[i] != default_state)
        count++;
    
    if (count) {
      int symno = nstates + sym - ntokens;
      int[] sp1 = froms[symno] = new int[count];
      int[] sp2 = tos[symno] = new int[count];

      foreach (i; begin..end)
        if (to_state[i] != default_state) {
          sp1[0] = from_state[i];
          sp1 = sp1[1..$];
          sp2[0] = to_state[i];
          sp2 = sp2[1..$];
        }
      
      tally[symno] = count;
      width[symno] = froms[symno][froms[symno].length - sp1.length - 1] - froms[symno][0] + 1;
    }
  }

  int default_goto(int sym, size_t[] state_count) {
    int begin = goto_map[sym - ntokens];
    int end = goto_map[sym - ntokens + 1];

    int res = 0;

    if (begin != end) {
      state_count[] = 0;

      foreach (i; begin..end)
        state_count[to_state[i]]++;

      size_t max = 0;
      foreach (s; 0..nstates)
        if (max < state_count[s]) {
          max = state_count[s];
          res = s;
        }
    }

    return res;
  }

  void goto_actions() {
    size_t[] state_count = new size_t[nstates];
    yydefgoto = new int[nnterms];

    foreach (i; ntokens..nsyms) {
      int default_state = default_goto(i, state_count);
      save_column(i, default_state);
      yydefgoto[i - ntokens] = default_state;
    }
  }

  token_actions;
  goto_actions;

  sort_actions;
  pack_table;

  return tuple(
    symbols,
    ntokens
  );
}
