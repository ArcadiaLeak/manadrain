module glslgram.selection_rest_statement;

import bison;

void selection_rest_statement() {
  declare_sym(symbol_get("selection_rest_statement"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("selection_rest_statement"));
  grammar_current_rule_symbol_append(symbol_get("statement_scoped"));
  grammar_current_rule_symbol_append(symbol_get("ELSE"));
  grammar_current_rule_symbol_append(symbol_get("statement_scoped"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("selection_rest_statement"));
  grammar_current_rule_symbol_append(symbol_get("statement_scoped"));
  grammar_current_rule_end();
}
