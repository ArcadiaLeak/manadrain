module glslgram.logical_and_expression;

import bison;

auto logical_and_expression() {
  declare_sym(symbol_get("logical_and_expression"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("logical_and_expression"));
  grammar_current_rule_symbol_append(symbol_get("inclusive_or_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("logical_and_expression"));
  grammar_current_rule_symbol_append(symbol_get("logical_and_expression"));
  grammar_current_rule_symbol_append(symbol_get("AND_OP"));
  grammar_current_rule_symbol_append(symbol_get("inclusive_or_expression"));
  grammar_current_rule_end();
}
