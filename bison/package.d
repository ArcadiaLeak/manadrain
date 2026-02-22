module bison;

public import bison.gram;
public import bison.json;
public import bison.reader;
public import bison.symlist;
public import bison.symtab;

enum bool TRACE_SETS = 0;
enum bool TRACE_CLOSURE = 0;
enum bool TRACE_AUTOMATON = 0;

void main(string[] args) {
  args[args.length - 1]
    .symbols_new.expand
    .read_json.expand
    .check_and_convert_grammar;
}
