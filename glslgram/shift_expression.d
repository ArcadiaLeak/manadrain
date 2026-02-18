module glslgram.shift_expression;

import bison;

auto shift_expression() {
  declare_sym(symbol_get("shift_expression"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("shift_expression"));
  grammar_current_rule_symbol_append(symbol_get("additive_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("shift_expression"));
  grammar_current_rule_symbol_append(symbol_get("shift_expression"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_OP"));
  grammar_current_rule_symbol_append(symbol_get("additive_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("shift_expression"));
  grammar_current_rule_symbol_append(symbol_get("shift_expression"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_OP"));
  grammar_current_rule_symbol_append(symbol_get("additive_expression"));
  grammar_current_rule_end();
}
