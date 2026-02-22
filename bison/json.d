module bison.json;
import bison;

import std.algorithm.iteration;
import std.array;
import std.container.dlist;
import std.file;
import std.json;
import std.path;
import std.typecons;

auto read_json(
  string gram_path,
  int nsyms,
  int ntokens,
  int nnterms,
  symbol[string] symbol_table,
  symbol acceptsymbol
) {
  int dummy_count = 0;
  int order_of_appearance = 0;
  int nritems = 0;
  int nrules = 0;

  symbol_list grammar;
  symbol_list start_symbols;

  symbol_list grammar_end;

  symbol_list current_rule;
  symbol_list previous_rule_end;

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

  symbol dummy_symbol_get() {
    import std.format;

    symbol sym = symbol_table.symbol_get(format("$@%d", ++dummy_count));
    sym.content.class_ = symbol_class_.nterm_sym;
    sym.order_of_appearance = ++order_of_appearance;

    return sym;
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

  void read_token() {
    string jsonString = gram_path
      .buildNormalizedPath("token.json")
      .readText;
    JSONValue jsonData = jsonString.parseJSON;

    foreach (tag; jsonData.array) {
      symbol sym = symbol_table.symbol_get(tag.str);
      sym.content.class_ = symbol_class_.token_sym;
      sym.order_of_appearance = ++order_of_appearance;
    }
  }

  void read_ritem(symbol sym, JSONValue ritem) {
    grammar_current_rule_begin(sym);
    
    string[long] tokenAssoc;
    if ("token" in ritem) {
      tokenAssoc = ritem["token"].array
        .map!(e => tuple(e["num"].integer, e["id"].str))
        .assocArray;
    }

    string[long] ntermAssoc;
    if ("nterm" in ritem) {
      ntermAssoc = ritem["nterm"].array
        .map!(e => tuple(e["num"].integer, e["id"].str))
        .assocArray;
    }

    void[][long] midruleAssoc;
    if ("midrule" in ritem) {
      midruleAssoc = ritem["midrule"].array
        .map!(e => tuple(e["num"].integer, []))
        .assocArray;
    }

    foreach (num; ritem["rhs"].array) {
      if (num.integer in tokenAssoc)
        grammar_current_rule_symbol_append(
          symbol_table.symbol_get(tokenAssoc[num.integer]));

      if (num.integer in ntermAssoc)
        grammar_current_rule_symbol_append(
          symbol_table.symbol_get(ntermAssoc[num.integer]));

      if (num.integer in midruleAssoc)
        grammar_midrule_action();
    }

    grammar_current_rule_end();
  }

  void read_nterm() {
    string jsonString = gram_path
      .buildNormalizedPath("nterm.json")
      .readText;
    JSONValue jsonData = jsonString.parseJSON;

    foreach (tag; jsonData["start"].array) {
      symbol sym = symbol_table.symbol_get(tag.str);
      start_symbols = new symbol_list(sym);
    }

    DList!JSONValue lhsJsonData = jsonData["inline"].array;

    foreach (imp; jsonData["import"].array) {
      string impJsonString = gram_path
        .buildNormalizedPath([imp.str, ".json"].join)
        .readText;
      JSONValue impJsonData = impJsonString.parseJSON;

      lhsJsonData ~= impJsonData.array;
    }

    JSONValue[][string] lhsAssocArray = assocArray(
      lhsJsonData[].map!(r => r["id"].str),
      lhsJsonData[].map!(r => r["rule"].array),
    );

    foreach (tag; jsonData["layout"].array) {
      symbol sym = symbol_table.symbol_get(tag.str);
      sym.content.class_ = symbol_class_.nterm_sym;
      sym.order_of_appearance = ++order_of_appearance;

      foreach (ritem; lhsAssocArray[tag.str])
        read_ritem(sym, ritem);
    }
  }

  read_token;
  read_nterm;

  return tuple(
    symbol_table,
    start_symbols,
    grammar,
    nrules,
    nritems,
    acceptsymbol
  );
}
