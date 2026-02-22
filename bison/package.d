module bison;

package import bison.derives;
package import bison.gram;
package import bison.json;
package import bison.nullable;
package import bison.reader;
package import bison.symlist;
package import bison.symtab;

enum bool TRACE_SETS = 0;
enum bool TRACE_CLOSURE = 0;
enum bool TRACE_AUTOMATON = 0;

void main(string[] args) {
  args[args.length - 1]
    .symbols_new.expand
    .read_json.expand
    .check_and_convert_grammar.expand
    .derives_compute.expand
    .nullable_compute;
}
