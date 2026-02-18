module glslgram.equality_expression;

import bison;

auto equality_expression() {
  declare_sym(symbol_get("equality_expression"), symbol_class_.nterm_sym);
  
  grammar_current_rule_begin(symbol_get("equality_expression"));
  grammar_current_rule_symbol_append(symbol_get("relational_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("equality_expression"));
  grammar_current_rule_symbol_append(symbol_get("equality_expression"));
  grammar_current_rule_symbol_append(symbol_get("EQ_OP"));
  grammar_current_rule_symbol_append(symbol_get("relational_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("equality_expression"));
  grammar_current_rule_symbol_append(symbol_get("equality_expression"));
  grammar_current_rule_symbol_append(symbol_get("NE_OP"));
  grammar_current_rule_symbol_append(symbol_get("relational_expression"));
  grammar_current_rule_end();
}
