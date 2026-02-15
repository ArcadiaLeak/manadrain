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

int nvectors;

base_number[][] froms;
base_number[][] tos;
int[][] conflict_tos;
size_t[] tally;
base_number[] width;

action_number[] actrow;

int[] conflrow;
int[] conflict_list;
int conflict_list_cnt;
int conflict_list_free;

rule_number[] yydefact;

void tables_generate() {
  nvectors = cast(int) nstates + nnterms;

  froms = new base_number[][nvectors];
  tos = new base_number[][nvectors];
  conflict_tos = new int[][nvectors];
  tally = new size_t[nvectors];
  width = new base_number[nvectors];

  token_actions;
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
    actrow[sym] = action_number(cast(int) shift_state.number);

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
