module glslgram.relational_expression;

import bison;

auto relational_expression() {
  declare_sym(symbol_get("relational_expression"), symbol_class_.nterm_sym);
  
  grammar_current_rule_begin(symbol_get("relational_expression"));
  grammar_current_rule_symbol_append(symbol_get("shift_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("relational_expression"));
  grammar_current_rule_symbol_append(symbol_get("relational_expression"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_ANGLE"));
  grammar_current_rule_symbol_append(symbol_get("shift_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("relational_expression"));
  grammar_current_rule_symbol_append(symbol_get("relational_expression"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_ANGLE"));
  grammar_current_rule_symbol_append(symbol_get("shift_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("relational_expression"));
  grammar_current_rule_symbol_append(symbol_get("relational_expression"));
  grammar_current_rule_symbol_append(symbol_get("LE_OP"));
  grammar_current_rule_symbol_append(symbol_get("shift_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("relational_expression"));
  grammar_current_rule_symbol_append(symbol_get("relational_expression"));
  grammar_current_rule_symbol_append(symbol_get("GE_OP"));
  grammar_current_rule_symbol_append(symbol_get("shift_expression"));
  grammar_current_rule_end();
}
