module bison.json;
import bison;

import std.algorithm.iteration;
import std.array;
import std.container.dlist;
import std.file;
import std.json;
import std.path;

void read_json(
  string gram_path,
  int nsyms,
  int ntokens,
  int nnterms,
  symbol[string] symbol_table
) {
  int order_of_appearance = 0;
  symbol_list start_symbols;

  {
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

  {
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
    }
  }
}
