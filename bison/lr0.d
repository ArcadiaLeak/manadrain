module bison.lr0;
import bison;

void generate_states(
  rule[] rules,
  int nrules,
  int nsyms,
  int ntokens,
  int nnterms,
  int nritems,
  symbol[] symbols,
  rule[][][] derives,
  int[] ritem
) {
  size_t[][] kernel_base;
  int[] kernel_size;
  size_t[] kernel_items;
  bool[] shift_symbol;
  rule[][] redset;
  state[] shiftset;

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

  allocate_storage;
  auto closure = closure_new(
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
}
