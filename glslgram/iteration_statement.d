module glslgram.iteration_statement;

import bison;

auto iteration_statement() {
  declare_sym(symbol_get("iteration_statement"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("iteration_statement"));
  grammar_current_rule_symbol_append(symbol_get("iteration_statement_nonattributed"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("iteration_statement"));
  grammar_current_rule_symbol_append(symbol_get("attribute"));
  grammar_current_rule_symbol_append(symbol_get("iteration_statement_nonattributed"));
  grammar_current_rule_end();
}
