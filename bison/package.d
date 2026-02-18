module bison;

private import bison.glslgram;
package import bison.closure;
package import bison.conflicts;
package import bison.derives;
package import bison.gram;
package import bison.lalr;
package import bison.lr0;
package import bison.nullable;
package import bison.reader;
package import bison.relation;
package import bison.state;
package import bison.symlist;
package import bison.symtab;
package import bison.tables;

enum bool TRACE_SETS = 0;
enum bool TRACE_CLOSURE = 0;
enum bool TRACE_AUTOMATON = 0;

void main(string[] args) {
  symbols_new;
  glslgram;
  check_and_convert_grammar;
  derives_compute;
  nullable_compute;
  generate_states;
  lalr;
  conflicts_solve;
  tables_generate;

  args[args.length - 1].write_output;  
}

void write_output(string filePath) {
  import std.file;
  import std.json;
  import std.path;
  import std.stdio;

  string dir = dirName(filePath);
  if (!dir.exists)
    mkdirRecurse(dir);
  File file = File(filePath, "w");

  JSONValue obj = JSONValue.emptyOrderedObject;
  foreach (sym; symbols[0..ntokens])
    obj[sym.symbol_id_get] = sym.content.code;

  file.writeln(obj.toString);
  file.close();
}
