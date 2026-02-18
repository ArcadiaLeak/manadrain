module glslgram.assignment_operator;

import bison;

private enum string[] RHSALT = [
  "EQUAL", "MUL_ASSIGN", "DIV_ASSIGN", "MOD_ASSIGN", "ADD_ASSIGN",
  "SUB_ASSIGN", "LEFT_ASSIGN", "RIGHT_ASSIGN", "AND_ASSIGN",
  "XOR_ASSIGN", "OR_ASSIGN"
];


auto assignment_operator() {
  declare_sym(symbol_get("assignment_operator"), symbol_class_.nterm_sym);

  static foreach (tag; RHSALT) {
    grammar_current_rule_begin(symbol_get("assignment_operator"));
    grammar_current_rule_symbol_append(symbol_get(tag));
    grammar_current_rule_end();
  }
}
