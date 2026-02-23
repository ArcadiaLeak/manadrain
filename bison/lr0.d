module bison.lr0;
import bison;

import std.typecons;

class state_list {
  state_list next;
  state state_;
}

auto generate_states(
  rule[] rules,
  int nrules,
  int nsyms,
  int ntokens,
  int nnterms,
  int nritems,
  symbol[] symbols,
  rule[][][] derives,
  int[] ritem,
  symbol acceptsymbol,
  bool[] nullable
) {
  size_t[][] kernel_base;
  int[] kernel_size;
  size_t[] kernel_items;
  bool[] shift_symbol;
  rule[][] redset;
  state[] shiftset;
  state_list first_state;
  state_list last_state;
  state final_state;
  state[] states;
  int nstates;
  state[size_t[]] state_table;

  void allocate_itemsets() {
    size_t count = 0;
    size_t[] symbol_count = new size_t[nsyms];

    for (int r = 0; r < nrules; ++r)
      for (size_t i = 0; rules[r].rhs[i] >= 0; i++) {
        int sym = rules[r].rhs[i];
        count += 1;
        symbol_count[sym] += 1;
      }

    kernel_base = new size_t[][nsyms];
    kernel_items = new size_t[count];

    count = 0;
    for (int i = 0; i < nsyms; i++) {
      kernel_base[i] = kernel_items[count..$];
      count += symbol_count[i];
    }

    kernel_size = new int[nsyms];
  }

  void allocate_storage() {
    allocate_itemsets();

    shiftset = new state[nsyms];
    redset = new rule[][nrules];
    shift_symbol = new bool[nsyms];
  }

  state state_list_append(int sym, size_t[] core) {
    state_list node = new state_list;
    state res = new state(sym, core.idup);
    res.number = nstates++;
    state_table[core.idup] = res;

    node.next = null;
    node.state_ = res;

    if (!first_state)
      first_state = node;
    if (last_state)
      last_state.next = node;
    last_state = node;

    return res;
  }

  allocate_storage;
  auto cl = new closure(
    nritems,
    nrules,
    ntokens,
    nnterms,
    nsyms,
    ritem,
    rules,
    derives,
    symbols
  );
  cl.set_fderives;

  void save_reductions(state s) {
    int count = 0;

    foreach (i; cl.itemset[0..cl.nitemset]) {
      int item = ritem[i];
      if (item < 0) {
        int r = -1 - item;
        redset[count++] = rules[r..$];
        if (r == 0)
          final_state = s;
      }
    }

    s.reductions = redset[0..count].dup;
  }
  
  void core_print(size_t[] core) {
    foreach (c; core) {
      import std.stdio;
      item_print(symbols, rules, ritem[c..$]);
      write("\n");
    }
  }

  void kernel_print() {
    for (int i = 0; i < nsyms; ++i)
      if (kernel_size[i]) {
        import std.stdio;
        writef("kernel[%s] =\n", symbols[i].tag);
        core_print(kernel_base[i][0..kernel_size[i]]);
      }
  }

  void new_itemsets(state s) {
    kernel_size[] = 0;
    shift_symbol[] = 0;

    foreach (i; cl.itemset[0..cl.nitemset]) {
      if (ritem[i] >= 0) {
        int sym = ritem[i];
        shift_symbol[sym] = true;
        kernel_base[sym][kernel_size[sym]] = i + 1;
        kernel_size[sym]++;
      }
    }

    if (TRACE_AUTOMATON) {
      import std.stdio;
      write("final kernel:\n");
      kernel_print();
      writef("new_itemsets: end: state = %d\n\n", s.number);
    }
  }

  state get_state(int sym, size_t[] core) {
    if (core !in state_table)
      state_list_append(sym, core);

    return state_table[core];
  }

  void append_states(state s) {
    import std.stdio;

    if (TRACE_AUTOMATON) 
      writef("append_states: begin: state = %d\n", s.number);

    int i = 0;
    foreach(sym, flag; shift_symbol)
      if (flag) {
        shiftset[i] = get_state(
          cast(int) sym,
          kernel_base[sym][0..kernel_size[sym]]
        );
        ++i;
      }

    if (TRACE_AUTOMATON) 
      writef("append_states: end: state = %d\n", s.number);
  }

  {
    kernel_size[0] = 0;
    for (int r = 0; r < nrules && rules[r].lhs.symbol_ == acceptsymbol; ++r)
      kernel_base[0][kernel_size[0]++] = ritem.length - rules[r].rhs.length;
    state_list_append(0, kernel_base[0][0..kernel_size[0]]);
  }

  for (state_list list = first_state; list; list = list.next) {
    state s = list.state_;

    cl.run_closure(s);
    save_reductions(s);
    new_itemsets(s);
    append_states(s);

    import std.algorithm.searching;
    symbols.state_transitions_set(s, shiftset[0..shift_symbol.count(1)]);
  }

  void set_states() {
    states = new state[nstates];

    while (first_state) {
      state_list self = first_state;

      state s = self.state_;
      states[s.number] = s;
      
      first_state = self.next;
    }
    first_state = null;
    last_state = null;
  }

  set_states;

  return tuple(
    states,
    ntokens,
    nnterms,
    nsyms,
    symbols,
    nullable
  );
}

void state_transitions_set(symbol[] symbols, state s, state[] dst) {
  s.transitions = dst.dup;
  if (TRACE_AUTOMATON)
    symbols.state_transitions_print(s);
}
