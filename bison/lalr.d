module bison.lalr;
import bison;

void lalr(
  state[] states,
  int ntokens,
  int nnterms,
  int nsyms
) {
  int state_lookaheads_count(state s) {
    rule[][] reds = s.reductions;
    state[] trans = s.transitions;

    s.consistent = !(
      reds.length > 1 ||
      (reds.length == 1 && trans.length && trans[0].accessing_symbol < ntokens)
    );

    return s.consistent ? 0 : cast(int) reds.length;
  }

  bool[][] LA;
  size_t nLA;

  void initialize_LA() {
    nLA = 0;
    foreach (cur_state; states)
      nLA += state_lookaheads_count(cur_state);

    if (!nLA)
      nLA = 1;

    import std.array;
    import std.range;

    bool[][] pLA = LA = new bool[nLA * ntokens]
      .chunks(ntokens)
      .array;

    foreach (cur_state; states) {
      int count = state_lookaheads_count(cur_state);

      if (count) {
        cur_state.lookaheads = pLA;
        pLA = pLA[count..$];
      }
    }
  }

  initialize_LA;
}
