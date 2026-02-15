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

void main() {
  symbols_new;
  glslgram;
  check_and_convert_grammar;
  derives_compute;
  nullable_compute;
  generate_states;
  lalr;
  conflicts_solve;
  tables_generate;
}
