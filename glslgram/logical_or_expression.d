module glslgram.logical_or_expression;

import bison;

auto logical_or_expression() {
  declare_sym(symbol_get("logical_or_expression"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("logical_or_expression"));
  grammar_current_rule_symbol_append(symbol_get("logical_xor_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("logical_or_expression"));
  grammar_current_rule_symbol_append(symbol_get("logical_or_expression"));
  grammar_current_rule_symbol_append(symbol_get("OR_OP"));
  grammar_current_rule_symbol_append(symbol_get("logical_xor_expression"));
  grammar_current_rule_end();
}
