module glslgram.multiplicative_expression;

import bison;

auto multiplicative_expression() {
  declare_sym(symbol_get("multiplicative_expression"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("multiplicative_expression"));
  grammar_current_rule_symbol_append(symbol_get("unary_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("multiplicative_expression"));
  grammar_current_rule_symbol_append(symbol_get("multiplicative_expression"));
  grammar_current_rule_symbol_append(symbol_get("STAR"));
  grammar_current_rule_symbol_append(symbol_get("unary_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("multiplicative_expression"));
  grammar_current_rule_symbol_append(symbol_get("multiplicative_expression"));
  grammar_current_rule_symbol_append(symbol_get("SLASH"));
  grammar_current_rule_symbol_append(symbol_get("unary_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("multiplicative_expression"));
  grammar_current_rule_symbol_append(symbol_get("multiplicative_expression"));
  grammar_current_rule_symbol_append(symbol_get("PERCENT"));
  grammar_current_rule_symbol_append(symbol_get("unary_expression"));
  grammar_current_rule_end();
}
