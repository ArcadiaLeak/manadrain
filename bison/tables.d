module bison.tables;
import bison;

struct base_number {
  int _;
  alias _ this;
}

struct action_number {
  int _;
  alias _ this;
}

struct vector_number {
  int _;
  alias _ this;
}

int nvectors;

base_number[][] froms;
base_number[][] tos;
int[][] conflict_tos;
size_t[] tally;
base_number[] width;

action_number[] actrow;

vector_number[] order;
int nentries;

base_number[] base;
base_number base_ninf = 0.base_number;

bool[] pos_set;
int pos_set_base = 0;

int[] conflrow;
int[] conflict_table;
int[] conflict_list;
int conflict_list_cnt;
int conflict_list_free;

enum int table_size = 32768;
base_number[] table;
base_number[] check;
base_number table_ninf = 0.base_number;

int lowzero;
int high;

state_number[] yydefgoto;
rule_number[] yydefact;

void tables_generate() {
  nvectors = nstates + nnterms;

  froms = new base_number[][nvectors];
  tos = new base_number[][nvectors];
  conflict_tos = new int[][nvectors];
  tally = new size_t[nvectors];
  width = new base_number[nvectors];

  token_actions;
  goto_actions;

  order = new vector_number[nvectors];
  sort_actions;
  pack_table;
}

void token_actions() {
  int nconflict = 0;

  yydefact = new rule_number[nstates];

  actrow = new action_number[ntokens];
  conflrow = new int[ntokens];

  conflict_list = new int[1 + 2 * nconflict];
  conflict_list_free = 2 * nconflict;
  conflict_list_cnt = 1;

  foreach (r; rules)
    r.useful = false;

  foreach (i; 0..nstates) {
    rule[] default_reduction = states[i].action_row;
    yydefact[i] = default_reduction ? default_reduction[0].number + 1 : 0;
    i.save_row;

    foreach (j; 0..ntokens)
      if (actrow[j] < 0 && actrow[j] != int.min)
        rules[-1 - actrow[j]].useful = true;
    if (yydefact[i])
      rules[yydefact[i] - 1].useful = true;
  }
}

void save_row(size_t s) {
  size_t count = 0;
  foreach (i; 0..ntokens)
    if (actrow[i] != 0)
      count++;
  
  if (count) {
    base_number[] sp1 = froms[s] = new base_number[count];
    base_number[] sp2 = tos[s] = new base_number[count];
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

    symbol_number sym = shift_state.accessing_symbol;
    
    if (actrow[sym] != 0) {
      conflicted = true;
      conflrow[sym] = 1;
    }
    actrow[sym] = action_number(shift_state.number);

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

void goto_actions() {
  size_t[] state_count = new size_t[nstates];
  yydefgoto = new state_number[nnterms];

  foreach (i; ntokens..nsyms) {
    state_number default_state = default_goto(i.symbol_number, state_count);
    save_column(i.symbol_number, default_state);
    yydefgoto[i - ntokens] = default_state;
  }
}

state_number default_goto(symbol_number sym, size_t[] state_count) {
  goto_number begin = goto_map[sym - ntokens];
  goto_number end = goto_map[sym - ntokens + 1];

  state_number res = 0.state_number;

  if (begin != end) {
    state_count[] = 0;

    foreach (i; begin..end)
      state_count[to_state[i]]++;

    size_t max = 0;
    foreach (s; 0..nstates)
      if (max < state_count[s]) {
        max = state_count[s];
        res = s.state_number;
      }
  }

  return res;
}

void save_column(symbol_number sym, state_number default_state) {
  goto_number begin = goto_map[sym - ntokens];
  goto_number end = goto_map[sym - ntokens + 1];

  size_t count = 0;
  foreach (i; begin..end)
    if (to_state[i] != default_state)
      count++;
  
  if (count) {
    vector_number symno = vector_number(nstates + sym - ntokens);
    base_number[] sp1 = froms[symno] = new base_number[count];
    base_number[] sp2 = tos[symno] = new base_number[count];

    foreach (i; begin..end)
      if (to_state[i] != default_state) {
        sp1[0] = base_number(from_state[i]);
        sp1 = sp1[1..$];
        sp2[0] = base_number(to_state[i]);
        sp2 = sp2[1..$];
      }
    
    tally[symno] = count;
    width[symno] = froms[symno][froms[symno].length - sp1.length - 1] - froms[symno][0] + 1;
  }
}

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

void pack_table() {
  base = new base_number[nvectors];
  pos_set = new bool[table_size + nstates];
  pos_set_base = -nstates;
  table = new base_number[table_size];
  conflict_table = new int[table_size];
  check = new base_number[table_size];

  lowzero = 0;
  high = 0;

  base[] = base_number(int.min);
  check[] = base_number(-1);

  foreach (i; 0..nentries) {
    state_number s = i.vector_number.matching_state;
    base_number place;

    if (s < 0)
      place = i.vector_number.pack_vector;
    else
      place = base[s];

    place.pos_set_set;
    base[order[i]] = place;
  }

  base_ninf = table_ninf_remap(base);
  table_ninf = table_ninf_remap(table[0..high + 1]);
}

state_number matching_state(vector_number vector) {
  vector_number i = order[vector];

  if (i < nstates) {
    size_t t = tally[i];
    int w = width[i];

    if (conflict_tos[i] !is null)
      foreach (j; 0..t)
        if (conflict_tos[i][j] != 0)
          return state_number(-1);
    
    foreach_reverse (prev; 0..vector) {
      vector_number j = order[prev];
      if (width[j] != w || tally[j] != t)
        return state_number(-1);
      else {
        bool match = true;
        foreach (k; 0..t) {
          if (!match) break;
          if (tos[j][k] != tos[i][k] ||
            froms[j][k] != froms[i][k] ||
            (conflict_tos[j] !is null && conflict_tos[j][k] != 0))
            match = false;
        }
        if (match) return state_number(j);
      }
    }
  }

  return state_number(-1);
}

base_number pack_vector(vector_number vector) {
  vector_number i = order[vector];
  size_t t = tally[i];
  base_number[] from = froms[i];
  base_number[] to = tos[i];
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

      if (ok && res.pos_set_test)
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
      
      return res.base_number;
    }
  }

  assert(0);
}

bool pos_set_test(int pos) {
  int bitno = pos - pos_set_base;
  return pos_set[bitno];
}

void pos_set_set(int pos) {
  int bitno = pos - pos_set_base;
  pos_set[bitno] = true;
}

base_number table_ninf_remap(base_number[] tab) {
  base_number res;

  foreach (t; tab)
    if (t < res && t != int.min)
      res = t;

  --res;

  foreach (i; 0..tab.length)
    if (tab[i] == int.min)
      tab[i] = res;

  return res;
}
