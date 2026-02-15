module bison.reader;
import bison;

symbol_list grammar;
symbol_list start_symbols;

symbol_list grammar_end;

symbol_list current_rule;
symbol_list previous_rule_end;

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

symbol_list grammar_symbol_append(symbol sym) {
  symbol_list p = new symbol_list(sym);

  if (grammar_end)
    grammar_end.next = p;
  else
    grammar = p;

  grammar_end = p;

  if (sym)
    ++nritems;

  return p;
}

void grammar_start_symbols_add(symbol_list syms) {
  start_symbols = syms;
}

void grammar_current_rule_begin(symbol lhs) {
  ++nrules;
  previous_rule_end = grammar_end;

  current_rule = grammar_symbol_append(lhs);

  if (lhs.content.class_ == symbol_class_.unknown_sym ||
    lhs.content.class_ == symbol_class_.percent_type_sym) {
    lhs.content.class_ = symbol_class_.nterm_sym;
  }
}

void grammar_current_rule_symbol_append(symbol sym) {
  grammar_symbol_append(sym);
}

void grammar_current_rule_end() {
  grammar_symbol_append(null);
}

void grammar_midrule_action() {
  symbol dummy = dummy_symbol_get();
  symbol_list midrule = new symbol_list(dummy);

  ++nrules;
  ++nritems;
  
  if (previous_rule_end)
    previous_rule_end.next = midrule;
  else
    grammar = midrule;

  midrule.next = new symbol_list(null);
  midrule.next.next = current_rule;

  previous_rule_end = midrule.next;

  grammar_current_rule_symbol_append(dummy);
}

void packgram() {
  import std.range.primitives;

  int itemno = 0;
  ritem = new item_number[nritems];

  int ruleno = 0;
  rules = new rule[nrules];

  for (symbol_list p = grammar; p; p = p.next) {
    symbol_list lhs = p;

    rules[ruleno].number = rule_number(ruleno);
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

void symbols_new() {
  acceptsymbol = symbol_get("$accept");
  acceptsymbol.order_of_appearance = 0;
  acceptsymbol.content.class_ = symbol_class_.nterm_sym;
  acceptsymbol.content.number = nnterms++;

  errtoken = symbol_get("YYerror");
  errtoken.order_of_appearance = 0;
  errtoken.content.class_ = symbol_class_.token_sym;
  errtoken.content.number = ntokens++;
  {
    symbol alias_ = symbol_get("error");
    alias_.order_of_appearance = 0;
    alias_.content.class_ = symbol_class_.token_sym;
    errtoken.make_alias(alias_);
  }

  undeftoken = symbol_get("YYUNDEF");
  undeftoken.order_of_appearance = 0;
  undeftoken.content.class_ = symbol_class_.token_sym;
  undeftoken.content.number = ntokens++;
  {
    symbol alias_ = symbol_get("$undefined");
    alias_.order_of_appearance = 0;
    alias_.content.class_ = symbol_class_.token_sym;
    undeftoken.make_alias(alias_);
  }
}

void check_and_convert_grammar() {
  eoftoken = symbol_get("YYEOF");
  eoftoken.order_of_appearance = 0;
  eoftoken.content.class_ = symbol_class_.token_sym;
  eoftoken.content.number = 0;
  eoftoken.content.code = 0;
  {
    symbol alias_ = symbol_get("$end");
    alias_.order_of_appearance = 0;
    alias_.content.class_ = symbol_class_.token_sym;
    eoftoken.make_alias(alias_);
  }

  create_start_rules;
  symbols_check_defined;

  nsyms = ntokens + nnterms;

  symbols_pack;
  packgram;
}

void create_start_rules() {
  symbol start = start_symbols.sym;
  create_start_rule(null, start);
}
