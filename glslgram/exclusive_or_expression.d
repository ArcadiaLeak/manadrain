module glslgram.exclusive_or_expression;

import bison;

void exclusive_or_expression() {
  declare_sym(symbol_get("exclusive_or_expression"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("exclusive_or_expression"));
  grammar_current_rule_symbol_append(symbol_get("and_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("exclusive_or_expression"));
  grammar_current_rule_symbol_append(symbol_get("exclusive_or_expression"));
  grammar_current_rule_symbol_append(symbol_get("CARET"));
  grammar_current_rule_symbol_append(symbol_get("and_expression"));
  grammar_current_rule_end();
}
