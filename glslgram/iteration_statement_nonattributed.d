module glslgram.iteration_statement_nonattributed;

import bison;

auto iteration_statement_nonattributed() {
  declare_sym(symbol_get("iteration_statement_nonattributed"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("iteration_statement_nonattributed"));
  grammar_current_rule_symbol_append(symbol_get("WHILE"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_PAREN"));
  grammar_midrule_action();
  grammar_current_rule_symbol_append(symbol_get("condition"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_symbol_append(symbol_get("statement_no_new_scope"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("iteration_statement_nonattributed"));
  grammar_current_rule_symbol_append(symbol_get("DO"));
  grammar_midrule_action();
  grammar_current_rule_symbol_append(symbol_get("statement"));
  grammar_current_rule_symbol_append(symbol_get("WHILE"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_PAREN"));
  grammar_current_rule_symbol_append(symbol_get("expression"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("iteration_statement_nonattributed"));
  grammar_current_rule_symbol_append(symbol_get("FOR"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_PAREN"));
  grammar_midrule_action();
  grammar_current_rule_symbol_append(symbol_get("for_init_statement"));
  grammar_current_rule_symbol_append(symbol_get("for_rest_statement"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_symbol_append(symbol_get("statement_no_new_scope"));
  grammar_current_rule_end();
}
