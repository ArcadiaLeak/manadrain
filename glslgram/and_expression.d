module glslgram.and_expression;

import bison;

auto and_expression() {
  declare_sym(symbol_get("and_expression"), symbol_class_.nterm_sym);
  
  grammar_current_rule_begin(symbol_get("and_expression"));
  grammar_current_rule_symbol_append(symbol_get("equality_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("and_expression"));
  grammar_current_rule_symbol_append(symbol_get("and_expression"));
  grammar_current_rule_symbol_append(symbol_get("AMPERSAND"));
  grammar_current_rule_symbol_append(symbol_get("equality_expression"));
  grammar_current_rule_end();
}
