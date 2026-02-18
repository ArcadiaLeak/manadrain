module glslgram.conditional_expression;

import bison;

auto conditional_expression() {
  declare_sym(symbol_get("conditional_expression"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("conditional_expression"));
  grammar_current_rule_symbol_append(symbol_get("logical_or_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("conditional_expression"));
  grammar_current_rule_symbol_append(symbol_get("logical_or_expression"));
  grammar_current_rule_symbol_append(symbol_get("QUESTION"));
  grammar_midrule_action();
  grammar_current_rule_symbol_append(symbol_get("expression"));
  grammar_current_rule_symbol_append(symbol_get("COLON"));
  grammar_current_rule_symbol_append(symbol_get("assignment_expression"));
  grammar_current_rule_end();
}
