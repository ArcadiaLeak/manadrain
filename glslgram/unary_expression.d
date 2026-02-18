module glslgram.unary_expression;

import bison;

auto unary_expression() {
  declare_sym(symbol_get("unary_expression"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("unary_expression"));
  grammar_current_rule_symbol_append(symbol_get("postfix_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("unary_expression"));
  grammar_current_rule_symbol_append(symbol_get("INC_OP"));
  grammar_current_rule_symbol_append(symbol_get("unary_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("unary_expression"));
  grammar_current_rule_symbol_append(symbol_get("DEC_OP"));
  grammar_current_rule_symbol_append(symbol_get("unary_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("unary_expression"));
  grammar_current_rule_symbol_append(symbol_get("unary_operator"));
  grammar_current_rule_symbol_append(symbol_get("unary_expression"));
  grammar_current_rule_end();
}
