module bison.lalr;
import bison;

class goto_list {
  goto_list next;
  int value;

  this(int v, goto_list n) {
    next = n;
    value = v;
  }
}

void lalr(
  state[] states,
  int ntokens,
  int nnterms,
  int nsyms,
  symbol[] symbols,
  bool[] nullable,
  rule[][][] derives,
  rule[] rules
) {
  bool[][] LA;
  size_t nLA;
  int[] goto_map;
  int ngotos;
  int[] from_state;
  int[] to_state;
  bool[][] goto_follows;
  int[][] includes;
  goto_list[] lookback;

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

  void follows_print(string title) {
    import std.stdio;
    writef("%s:\n", title);
    foreach (i; 0..ngotos) {
      write("    FOLLOWS[");
      goto_print(i);
      write("] =");
      foreach (sym, flag; goto_follows[i])
        if (flag)
          writef(" %s", symbols[sym].tag);
      write("\n");
    }
    write("\n");
  }

  int map_goto(int src, int sym) {
    int low = goto_map[sym - ntokens];
    int high = goto_map[sym - ntokens + 1];
    high -= 1;

    while (true) {
      int middle = (low + high) / 2;
      int s = from_state[middle];
      if (s == src)
        return middle;
      else if (s < src)
        low = middle + 1;
      else
        high = middle - 1;
    }
  }

  void initialize_goto_follows() {
    int[][] reads = new int[][ngotos];
    int[] edge = new int[ngotos];
    
    import std.array;
    import std.range;
    goto_follows = new bool[ngotos * ntokens].chunks(ntokens).array;

    foreach (size_t i; 0..ngotos) {
      int dst = to_state[i];
    
      state[] trans = states[dst].transitions;
      while (trans.length > 0) {
        if (trans[0] is null)
          continue;
        if (trans[0].accessing_symbol >= ntokens)
          break;

        goto_follows[i][trans[0].accessing_symbol] = true;
        trans = trans[1..$];
      }
        
      int nedges = 0;
      while (trans.length > 0) {
        int sym = trans[0].accessing_symbol;
        if (nullable[sym - ntokens])
          edge[nedges++] = map_goto(dst, sym);
        trans = trans[1..$];
      }

      if (nedges == 0)
        reads[i] = null;
      else {
        reads[i] = edge[0..nedges];
        reads[i].length++;
        reads[i][nedges] = -1;
      }
    }

    if (TRACE_AUTOMATON) {
      follows_print("follows after shifts");
      relation_print!goto_print("reads", reads);
    }

    relation_digraph(reads, goto_follows);
    if (TRACE_AUTOMATON)
      follows_print("follows after read");
  }

  void add_lookback_edge(state s, rule[] r, int gotono) {
    int ri = state_reduction_find(s, r);
    int idx = cast(int) (LA.length - s.lookaheads.length) + ri;
    lookback[idx] = new goto_list(gotono, lookback[idx]);
  }

  void build_relations() {
    int[] edge = new int[ngotos];
    int[] path = new int[rules.ritem_longest_rhs + 1];

    includes = new int[][ngotos];

    foreach (i; 0..ngotos) {
      int src = from_state[i];
      int dst = to_state[i];
      int var = states[dst].accessing_symbol;

      int nedges = 0;
      foreach (r; derives[var - ntokens]) {
        if (r is null)
          break;
        state s = states[src];
        path[0] = s.number;

        int length = 1;
        for (int rp = 0; r[0].rhs[rp] >= 0; rp++) {
          int sym = r[0].rhs[rp];
          s = transitions_to(s, sym);
          path[length++] = s.number;
        }

        if (!s.consistent)
          add_lookback_edge(s, r, i);

        foreach_reverse (p; 0..length - 1) {
          if (r[0].rhs[p] < ntokens)
            break;
          int sym = r[0].rhs[p];
          int g = map_goto(path[p], sym);
          {
            bool found = false;
            foreach (j; 0..nedges)
              found = edge[j] == g;
            if (!found)
              edge[nedges++] = g;
          }
          if (!nullable[sym - ntokens])
            break;
        }
      }

      if (TRACE_AUTOMATON) {
        import std.stdio;
        goto_print(i);
        write(" edges = ");
        foreach (j; 0..nedges) {
          write(" ");
          goto_print(edge[j]);
        }
        write("\n");
      }

      if (nedges == 0)
        includes[i] = null;
      else {
        includes[i] = new int[nedges + 1];
        foreach (j; 0..nedges)
          includes[i][j] = edge[j];
        includes[i][nedges] = -1;
      }
    }

    includes.relation_transpose;
    if (TRACE_AUTOMATON)
      relation_print!goto_print("includes", includes);
  }

  initialize_LA;
  set_goto_map;
  initialize_goto_follows;
  lookback = new goto_list[nLA];
  build_relations;
}
