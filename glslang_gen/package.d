module glslang_gen;

import bison;
import glslgram;

import std.array;
import std.file;
import std.format;
import std.path;
import std.stdio;

void write_output() {
  string dir = buildPath(getcwd, "glslang_tab");
  if (!dir.exists)
    mkdirRecurse(dir);
  File file = File(dir.buildPath("yytokentype.d"), "w");
  
  file.write("module glslang_tab.yytokentype;");
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
      sym.symbol_id_get,
      sym.content.code
    );

    rowbuf ~= row;
    nrowbuf += row.length;
  }

  file.write(colbuf[].join(",\n  "));
  file.write("\n\x7D\n");
  file.close();
}

void main() {
  bison_!glslgram_;
  write_output;
}
