module bison;

public import bison.closure;
public import bison.conflicts;
public import bison.derives;
public import bison.gram;
public import bison.lalr;
public import bison.lr0;
public import bison.nullable;
public import bison.reader;
public import bison.relation;
public import bison.state;
public import bison.symlist;
public import bison.symtab;
public import bison.tables;

enum bool TRACE_SETS = 0;
enum bool TRACE_CLOSURE = 0;
enum bool TRACE_AUTOMATON = 0;

void bison_(alias gram)() {
  symbols_new;
  gram;
  check_and_convert_grammar;
  derives_compute;
  nullable_compute;
  generate_states;
  lalr;
  conflicts_solve;
  tables_generate;
}
