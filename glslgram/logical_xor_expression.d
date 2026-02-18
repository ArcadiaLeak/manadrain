module glslgram.logical_xor_expression;

import bison;

auto logical_xor_expression() {
  declare_sym(symbol_get("logical_xor_expression"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("logical_xor_expression"));
  grammar_current_rule_symbol_append(symbol_get("logical_and_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("logical_xor_expression"));
  grammar_current_rule_symbol_append(symbol_get("logical_xor_expression"));
  grammar_current_rule_symbol_append(symbol_get("XOR_OP"));
  grammar_current_rule_symbol_append(symbol_get("logical_and_expression"));
  grammar_current_rule_end();
}
