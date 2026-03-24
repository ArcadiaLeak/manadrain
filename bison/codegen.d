module bison.codegen;
import bison;

import std.algorithm.iteration;
import std.container.dlist;
import std.file;
import std.format;
import std.path;
import std.range;
import std.stdio;

void write_yytokentype(
  symbol[] symbols,
  int ntokens
) {
  string dir = buildPath(getcwd, "bison_tab");
  if (!dir.exists)
    mkdirRecurse(dir);
  File file = File(dir.buildPath("yytokentype.d"), "w");
  
  file.write("module bison_tab.yytokentype;");
  file.write("\n\n");
  file.write("enum yytokentype_ \x7B");
  file.write("\n  ");

  string build_enum_id(symbol sym) {
    if (sym.alias_)
      return sym.alias_.tag;
    else
      return sym.tag;
  }

  string build_enum_member(symbol sym) {
    return format!"%s = %d"(
      build_enum_id(sym),
      sym.content.code
    );
  }

  string build_enum_line(symbol[] line_syms) {
    return line_syms
      .map!build_enum_member
      .join(", ");
  }

  file.write(
    symbols[0..ntokens]
      .chunks(4)
      .map!build_enum_line
      .join(",\n  ")
  );

  file.write("\n\x7D\n");
  file.close();
}
