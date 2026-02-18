module glslgram.statement_no_new_scope;

import bison;

auto statement_no_new_scope() {
  declare_sym(symbol_get("statement_no_new_scope"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("statement_no_new_scope"));
  grammar_current_rule_symbol_append(symbol_get("compound_statement_no_new_scope"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("statement_no_new_scope"));
  grammar_current_rule_symbol_append(symbol_get("simple_statement"));
  grammar_current_rule_end();
}
