module bison.codegen;
import bison;

import std.array;
import std.file;
import std.format;
import std.path;
import std.stdio;

void write_yytokentype(
  symbol[] symbols,
  int ntokens
) {
  string symbol_id_get(symbol sym) {
    if (sym.alias_)
      return sym.alias_.tag;
    else
      return sym.tag;
  }

  string dir = buildPath(getcwd, "bison_tab");
  if (!dir.exists)
    mkdirRecurse(dir);
  File file = File(dir.buildPath("yytokentype.d"), "w");
  
  file.write("module bison_tab.yytokentype;");
  file.write("\n\n");
  file.write("enum yytokentype_ \x7B");
  file.write("\n  ");

  Appender!(string[]) colbuf;
  Appender!(string[]) rowbuf;
  size_t nrowbuf;

  foreach (sym; symbols[0..ntokens]) {
    size_t nrowtotal = nrowbuf;
    if (rowbuf.length > 0)
      nrowtotal += rowbuf.length * 2 - 2;

    if (nrowtotal > 80) {
      colbuf ~= rowbuf[].join(", ");
      rowbuf.clear;
      nrowbuf = 0;
    }

    string row = format!"%s = %d"(
      symbol_id_get(sym),
      sym.content.code
    );

    rowbuf ~= row;
    nrowbuf += row.length;
  }

  file.write(colbuf[].join(",\n  "));
  file.write("\n\x7D\n");
  file.close();
}
