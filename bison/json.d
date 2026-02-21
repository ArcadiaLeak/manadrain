module bison.json;
import bison;

import std.algorithm.iteration;
import std.array;
import std.container.dlist;
import std.file;
import std.json;
import std.path;
import std.stdio;

void read_json(
  string gram_path,
  int nsyms,
  int ntokens,
  int nnterms,
  symbol[string] symbol_table
) {
  string jsonString = gram_path
    .buildNormalizedPath("nterm.json")
    .readText;
  JSONValue jsonData = jsonString.parseJSON;

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

  foreach (lhs; jsonData["layout"].array) {
    writeln(lhs.str);
  }
}
