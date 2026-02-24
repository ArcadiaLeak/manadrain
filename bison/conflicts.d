module bison.conflicts;
import bison;

import std.typecons;

auto conflicts_solve(
  int nstates,
  int ntokens,
  int nnterms,
  int nsyms,
  state[] states,
  rule[] rules,
  symbol errtoken,
  int[] goto_map,
  int[] from_state,
  int[] to_state
) {
  symbol[] errors = new symbol[ntokens + 1];
  
  bool[] conflicts = new bool[nstates];
  bool[] shift_set = new bool[ntokens];
  bool[] lookahead_set = new bool[ntokens];

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

  foreach (s; states)
    set_conflicts(s, errors);

  return tuple(
    ntokens,
    nnterms,
    nsyms,
    nstates,
    states,
    rules,
    errtoken,
    goto_map,
    from_state,
    to_state
  );
}
