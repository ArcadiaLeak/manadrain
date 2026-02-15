module bison.conflicts;
import bison;

bool[] conflicts;

bool[] shift_set;
bool[] lookahead_set;

void conflicts_solve() {
  symbol[] errors = new symbol[ntokens + 1];

  conflicts = new bool[nstates];
  shift_set = new bool[ntokens];
  lookahead_set = new bool[ntokens];

  foreach (s; states)
    set_conflicts(s, errors);
}

void set_conflicts(state s, symbol[] errors) {
  if (s.consistent)
    return;

  lookahead_set[] = 0;

  state[] trans = s.transitions;
  while (trans.length > 0) {
    if (trans[0] is null)
      continue;
    if (trans[0].accessing_symbol >= ntokens)
      break;

    lookahead_set[trans[0].accessing_symbol] = true;
    trans = trans[1..$];
  }

  if (s.lookaheads)
    foreach (i; 0..s.reductions.length) {
      bool[] LAxsect = new bool[ntokens];
      LAxsect[] = s.lookaheads[i][] & lookahead_set[];

      import std.algorithm.searching;
      if (LAxsect.any)
        conflicts[s.number] = true;
      
      lookahead_set[] |= s.lookaheads[i][];
    }
}