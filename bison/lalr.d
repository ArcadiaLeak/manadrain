module bison.lalr;
import bison;

struct goto_number {
  size_t _;
  alias _ this;
}

goto_number[] goto_map;
goto_number ngotos;
state_number[] from_state;
state_number[] to_state;
bool[][] goto_follows;

class goto_list {
  goto_list next;
  goto_number value;

  this(goto_number v, goto_list n) {
    next = n;
    value = v;
  }
}

bool[][] LA;
size_t nLA;

goto_number[][] includes;
goto_list[] lookback;

void lalr() {
  initialize_LA;
  set_goto_map;
  initialize_goto_follows;
  lookback = new goto_list[nLA];
  build_relations;
  compute_follows;
  compute_lookaheads;

  if (TRACE_SETS)
    lookaheads_print;
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

int state_lookaheads_count(state s) {
  rule[][] reds = s.reductions;
  state[] trans = s.transitions;

  s.consistent = !(
    reds.length > 1 ||
    (reds.length == 1 && trans.length && trans[0].accessing_symbol < ntokens)
  );

  return s.consistent ? 0 : cast(int) reds.length;
}

void set_goto_map() {
  goto_map = new goto_number[nnterms + 1];
  ngotos = goto_number(0);

  foreach (s; states) {
    import std.range;
    foreach_reverse (trans; s.transitions) {
      if (trans.accessing_symbol < ntokens) break;
      ngotos++;
      goto_map[trans.accessing_symbol - ntokens]++;
    }
  }

  goto_number[] temp_map = new goto_number[nnterms + 1];
  {
    goto_number k = goto_number(0);
    for (size_t i = ntokens; i < nsyms; ++i) {
      temp_map[i - ntokens] = k;
      k += goto_map[i - ntokens];
    }

    for (size_t i = ntokens; i < nsyms; ++i)
      goto_map[i - ntokens] = temp_map[i - ntokens];

    goto_map[nsyms - ntokens] = ngotos;
    temp_map[nsyms - ntokens] = ngotos;
  }

  from_state = new state_number[ngotos];
  to_state = new state_number[ngotos];

  foreach (s_idx, s; states) {
    import std.range;
    foreach_reverse (trans; s.transitions) {
      if (trans.accessing_symbol < ntokens) break;
      goto_number k = temp_map[trans.accessing_symbol - ntokens]++;
      from_state[k] = state_number(cast(int) s_idx);
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
      i.goto_print;
      write("\n");
    }
  }
}

void goto_print(size_t i) {
  import std.stdio;
  state_number src = from_state[i];
  state_number dst = to_state[i];
  symbol_number var = states[dst].accessing_symbol;
  writef("goto[%d] = (%d, %s, %d)", i, src, symbols[var].tag, dst);
}

void initialize_goto_follows() {
  goto_number[][] reads = new goto_number[][ngotos];
  goto_number[] edge = new goto_number[ngotos];
  
  import std.array;
  import std.range;
  goto_follows = new bool[ngotos * ntokens].chunks(ntokens).array;

  foreach (size_t i; 0..ngotos) {
    state_number dst = to_state[i];
  
    state[] trans = states[dst].transitions;
    while (trans.length > 0) {
      if (trans[0] is null)
        continue;
      if (trans[0].accessing_symbol >= ntokens)
        break;

      goto_follows[i][trans[0].accessing_symbol] = true;
      trans = trans[1..$];
    }
      
    goto_number nedges = goto_number(0);
    while (trans.length > 0) {
      symbol_number sym = trans[0].accessing_symbol;
      if (nullable[sym - ntokens])
        edge[nedges++] = map_goto(dst, sym);
      trans = trans[1..$];
    }

    if (nedges == 0)
      reads[i] = null;
    else {
      reads[i] = edge[0..nedges];
      reads[i].length++;
      reads[i][nedges] = goto_number(-1);
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

goto_number map_goto(state_number src, symbol_number sym) {
  goto_number low = goto_map[sym - ntokens];
  goto_number high = goto_map[sym - ntokens + 1];
  high -= 1;

  while (true) {
    goto_number middle = goto_number((low + high) / 2);
    state_number s = from_state[middle];
    if (s == src)
      return middle;
    else if (s < src)
      low = middle + 1;
    else
      high = middle - 1;
  }
}

void follows_print(string title) {
  import std.stdio;
  writef("%s:\n", title);
  foreach (i; 0..ngotos) {
    write("    FOLLOWS[");
    goto_print(i.goto_number);
    write("] =");
    foreach (sym, flag; goto_follows[i])
      if (flag)
        writef(" %s", symbols[sym].tag);
    write("\n");
  }
  write("\n");
}

void build_relations() {
  goto_number[] edge = new goto_number[ngotos];
  state_number[] path = new state_number[ritem_longest_rhs + 1];

  includes = new goto_number[][ngotos];

  foreach (i; 0..ngotos) {
    state_number src = from_state[i];
    state_number dst = to_state[i];
    symbol_number var = states[dst].accessing_symbol;

    int nedges = 0;
    foreach (r; derives[var - ntokens]) {
      if (r is null)
        break;
      state s = states[src];
      path[0] = s.number;

      int length = 1;
      for (int rp = 0; r[0].rhs[rp] >= 0; rp++) {
        symbol_number sym = symbol_number(r[0].rhs[rp]);
        s = transitions_to(s, sym);
        path[length++] = s.number;
      }

      if (!s.consistent)
        add_lookback_edge(s, r, i.goto_number);

      foreach_reverse (p; 0..length - 1) {
        if (r[0].rhs[p] < ntokens)
          break;
        symbol_number sym = symbol_number(r[0].rhs[p]);
        goto_number g = map_goto(path[p], sym);
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
      i.goto_print;
      write(" edges = ");
      foreach (j; 0..nedges) {
        write(" ");
        edge[j].goto_print;
      }
      write("\n");
    }

    if (nedges == 0)
      includes[i] = null;
    else {
      includes[i] = new goto_number[nedges + 1];
      foreach (j; 0..nedges)
        includes[i][j] = edge[j];
      includes[i][nedges] = -1;
    }
  }

  includes.relation_transpose;
  if (TRACE_AUTOMATON)
    relation_print!goto_print("includes", includes);
}

void add_lookback_edge(state s, rule[] r, goto_number gotono) {
  int ri = state_reduction_find(s, r);
  int idx = cast(int) (LA.length - s.lookaheads.length) + ri;
  lookback[idx] = new goto_list(gotono, lookback[idx]);
}

void compute_follows() {
  relation_digraph(includes, goto_follows);
  if (TRACE_SETS)
    follows_print("follows after includes");
}

void lookback_print() {
  import std.stdio;
  write("lookback:\n");
  foreach (i; 0..nLA)
    if (lookback[i]) {
      writef("   %3d = ", i);
      state s = i.lookback_find_state;
      size_t rnum = i - (LA.length - s.lookaheads.length);
      rule[] r = s.reductions[rnum];
      writef("(%3d, ", s.number);
      r[0].rule_lhs_print;
      r[0].rule_rhs_print;
      write(") ->");
      for (goto_list sp = lookback[i]; sp; sp = sp.next) {
        write(" ");
        sp.value.goto_print;
      }
      write("\n");
    }
  write("\n");
}

state lookback_find_state(size_t lookback_index) {
  state res = null;
  foreach (s; states)
    if (s.reductions && s.lookaheads) {
      if (LA.length - s.lookaheads.length > lookback_index)
        break;
      else
        res = s;
    }
  if (!res)
    assert(0);
  return res;
}

void compute_lookaheads() {
  if (TRACE_AUTOMATON)
    lookback_print;
  foreach (i; 0..nLA)
    for (goto_list sp = lookback[i]; sp; sp = sp.next)
      LA[i][] |= goto_follows[sp.value][];
}

void lookaheads_print() {
  import std.stdio;
  write("Lookaheads:\n");
  foreach (i; 0..nstates) {
    rule[][] reds = states[i].reductions;
    if (reds.length) {
      writef("  State %d:\n", i);
      foreach (j; 0..reds.length) {
        writef("    rule %d:", reds[j][0].number);
        if (states[i].lookaheads)
          foreach (k, flag; states[i].lookaheads[j])
            if (flag)
              writef(" %s", symbols[k].tag);
        write("\n");
      }
    }
  }
  write("\n");
}
