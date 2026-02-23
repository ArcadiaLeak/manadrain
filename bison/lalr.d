module bison.lalr;
import bison;

void lalr(
  state[] states,
  int ntokens,
  int nnterms,
  int nsyms,
  symbol[] symbols
) {
  bool[][] LA;
  size_t nLA;
  int[] goto_map;
  int ngotos;
  int[] from_state;
  int[] to_state;

  int state_lookaheads_count(state s) {
    rule[][] reds = s.reductions;
    state[] trans = s.transitions;

    s.consistent = !(
      reds.length > 1 ||
      (reds.length == 1 && trans.length && trans[0].accessing_symbol < ntokens)
    );

    return s.consistent ? 0 : cast(int) reds.length;
  }

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

  void goto_print(size_t i) {
    import std.stdio;
    int src = from_state[i];
    int dst = to_state[i];
    int var = states[dst].accessing_symbol;
    writef("goto[%d] = (%d, %s, %d)", i, src, symbols[var].tag, dst);
  }

  void set_goto_map() {
    goto_map = new int[nnterms + 1];
    ngotos = 0;

    foreach (s; states) {
      import std.range;
      foreach_reverse (trans; s.transitions) {
        if (trans.accessing_symbol < ntokens) break;
        ngotos++;
        goto_map[trans.accessing_symbol - ntokens]++;
      }
    }

    int[] temp_map = new int[nnterms + 1];
    {
      int k = 0;
      for (size_t i = ntokens; i < nsyms; ++i) {
        temp_map[i - ntokens] = k;
        k += goto_map[i - ntokens];
      }

      for (size_t i = ntokens; i < nsyms; ++i)
        goto_map[i - ntokens] = temp_map[i - ntokens];

      goto_map[nsyms - ntokens] = ngotos;
      temp_map[nsyms - ntokens] = ngotos;
    }

    from_state = new int[ngotos];
    to_state = new int[ngotos];

    foreach (s_idx, s; states) {
      import std.range;
      foreach_reverse (trans; s.transitions) {
        if (trans.accessing_symbol < ntokens) break;
        int k = temp_map[trans.accessing_symbol - ntokens]++;
        from_state[k] = cast(int) s_idx;
        to_state[k] = trans.number;
      }
    }

    if (TRACE_AUTOMATON) {
      import std.stdio;
      for (size_t i = 0; i < nnterms; ++i)
        writef(
          "goto_map[%d (%s)] = %d .. %d\n",
          i, symbols[ntokens + i].tag,
          goto_map[i], cast(int) goto_map[i + 1] - 1
        );
      for (size_t i = 0; i < ngotos; ++i) {
        goto_print(i);
        write("\n");
      }
    }
  }

  initialize_LA;
  set_goto_map;
}
