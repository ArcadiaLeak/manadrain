module bison.reader;
import bison;

import std.typecons;

auto symbols_new(string gram_path) {
  int nsyms = 0;
  int ntokens = 1;
  int nnterms = 0;

  symbol[string] symbol_table;

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
    symbol_table
  );
}

void check_and_convert_grammar(
  symbol[string] symbol_table,
  symbol_list start_symbols,
  symbol_list grammar,
  int nrules,
  int nritems,
  symbol acceptsymbol
) {
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

  create_start_rules;
}
