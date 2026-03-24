module bison;

package import bison.closure;
package import bison.codegen;
package import bison.conflicts;
package import bison.derives;
package import bison.gram;
package import bison.json;
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
  args[$ - 1]
    .symbols_new.expand
    .read_json.expand
    .check_and_convert_grammar.expand
    .derives_compute.expand
    .nullable_compute.expand
    .generate_states.expand
    .lalr.expand
    .conflicts_solve.expand
    .tables_generate.expand
    .write_yytokentype;
}
