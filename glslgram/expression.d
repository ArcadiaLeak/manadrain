module glslgram.expression;

import bison;

auto expression() {
  declare_sym(symbol_get("expression"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("expression"));
  grammar_current_rule_symbol_append(symbol_get("assignment_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("expression"));
  grammar_current_rule_symbol_append(symbol_get("expression"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("assignment_expression"));
  grammar_current_rule_end();
}
