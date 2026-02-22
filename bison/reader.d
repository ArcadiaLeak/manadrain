module bison.reader;
import bison;

import std.typecons;

auto symbols_new(string gram_path) {
  int nsyms = 0;
  int ntokens = 1;
  int nnterms = 0;

  symbol[string] symbol_table = new symbol[string];

  symbol acceptsymbol = symbol_table.symbol_get("$accept");
  acceptsymbol.order_of_appearance = 0;
  acceptsymbol.content.class_ = symbol_class_.nterm_sym;
  acceptsymbol.content.number = nnterms++;

  symbol errtoken = symbol_table.symbol_get("YYerror");
  errtoken.order_of_appearance = 0;
  errtoken.content.class_ = symbol_class_.token_sym;
  errtoken.content.number = ntokens++;
  {
    symbol alias_ = symbol_table.symbol_get("error");
    alias_.order_of_appearance = 0;
    alias_.content.class_ = symbol_class_.token_sym;
    errtoken.make_alias(alias_);
  }

  symbol undeftoken = symbol_table.symbol_get("YYUNDEF");
  undeftoken.order_of_appearance = 0;
  undeftoken.content.class_ = symbol_class_.token_sym;
  undeftoken.content.number = ntokens++;
  {
    symbol alias_ = symbol_table.symbol_get("$undefined");
    alias_.order_of_appearance = 0;
    alias_.content.class_ = symbol_class_.token_sym;
    undeftoken.make_alias(alias_);
  }

  return tuple(
    gram_path,
    nsyms,
    ntokens,
    nnterms,
    symbol_table,
    acceptsymbol,
    undeftoken,
    errtoken
  );
}

void check_and_convert_grammar(
  symbol[string] symbol_table,
  symbol_list start_symbols,
  symbol_list grammar,
  int nrules,
  int nritems,
  symbol acceptsymbol,
  int ntokens,
  int nnterms,
  symbol undeftoken,
  symbol errtoken
) {
  symbol[] symbols_sorted;
  int[] token_translations;
  symbol[] symbols;
  int nsyms;
  int max_code;
  int[] ritem;
  rule[] rules;

  symbol eoftoken = symbol_table.symbol_get("YYEOF");
  eoftoken.order_of_appearance = 0;
  eoftoken.content.class_ = symbol_class_.token_sym;
  eoftoken.content.number = 0;
  eoftoken.content.code = 0;
  {
    symbol alias_ = symbol_table.symbol_get("$end");
    alias_.order_of_appearance = 0;
    alias_.content.class_ = symbol_class_.token_sym;
    eoftoken.make_alias(alias_);
  }

  void create_start_rule(symbol swtok, symbol start) {
    symbol_list initial_rule = new symbol_list(acceptsymbol);
    symbol_list p = initial_rule;
    p.next = new symbol_list(start);
    p = p.next;
    p.next = new symbol_list(eoftoken);
    p = p.next;
    p.next = new symbol_list(null);
    p = p.next;
    p.next = grammar;
    nrules += 1;
    nritems += 3;
    grammar = initial_rule;
  }

  void create_start_rules() {
    symbol start = start_symbols.sym;
    create_start_rule(null, start);
  }

  void symbols_check_defined() {
    import std.algorithm.sorting;
    import std.array;

    symbols_sorted = symbol_table.values
      .sort!("a.order_of_appearance < b.order_of_appearance")
      .array;

    foreach (sym; symbols_sorted)
      if (sym.content.number == NUMBER_UNDEFINED)
        sym.content.number = sym.content.class_ == symbol_class_.token_sym
          ? ntokens++ : nnterms++;
  }

  void symbol_pack(symbol sym) {
    if (sym.content.class_ == symbol_class_.nterm_sym)
      sym.content.number += ntokens;

    symbols[sym.content.number] = sym.content.symbol_;
  }

  void symbol_translation(symbol sym) {
    if (sym.content.class_ == symbol_class_.token_sym && !sym.is_alias)
      token_translations[sym.content.code] = sym.content.number;
  }

  void symbols_token_translations_init() {
    bool code_256_available_p = true;

    max_code = 0;
    foreach (s; symbols[0..ntokens]) {
      sym_content sym = s.content;
      if (sym.code != CODE_UNDEFINED) {
        if (sym.code > max_code)
          max_code = sym.code;
        if (sym.code == 256)
          code_256_available_p = false;
      }
    }

    if (code_256_available_p && errtoken.content.code == CODE_UNDEFINED)
      errtoken.content.code = 256;

    if (max_code < 256)
      max_code = 256;

    foreach (s; symbols[0..ntokens]) {
      sym_content sym = s.content;
      if (sym.code == CODE_UNDEFINED)
        sym.code = ++max_code;
      if (sym.code > max_code)
        max_code = sym.code;
    }

    token_translations = new int[max_code + 1];
    token_translations[] = undeftoken.content.number;

    foreach (sym; symbols_sorted)
      symbol_translation(sym);
  }

  void symbols_pack() {
    symbols = new symbol[nsyms];
    foreach (sym; symbols_sorted)
      symbol_pack(sym);

    symbols_token_translations_init;
  }

  void packgram() {
    import std.range.primitives;

    int itemno = 0;
    ritem = new int[nritems];

    int ruleno = 0;
    rules = new rule[nrules];

    for (symbol_list p = grammar; p; p = p.next) {
      symbol_list lhs = p;

      rules[ruleno].number = ruleno;
      rules[ruleno].lhs = lhs.sym.content;
      rules[ruleno].rhs = ritem[itemno..$];

      size_t rule_length = 0;
      for (p = lhs.next; p.sym; p = p.next) {
        ++rule_length;
        ritem[itemno++] = p.sym.content.number;
      }

      ritem[itemno++] = -1 - ruleno;
      ++ruleno;
    }
  }

  create_start_rules;
  symbols_check_defined;

  nsyms = ntokens + nnterms;

  symbols_pack;
  packgram;
}
