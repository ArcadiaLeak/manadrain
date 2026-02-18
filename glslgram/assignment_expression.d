module glslgram.assignment_expression;

import bison;

auto assignment_expression() {
  declare_sym(symbol_get("assignment_expression"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("assignment_expression"));
  grammar_current_rule_symbol_append(symbol_get("conditional_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("assignment_expression"));
  grammar_current_rule_symbol_append(symbol_get("unary_expression"));
  grammar_current_rule_symbol_append(symbol_get("assignment_operator"));
  grammar_current_rule_symbol_append(symbol_get("assignment_expression"));
  grammar_current_rule_end();
}
