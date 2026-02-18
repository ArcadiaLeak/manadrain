module glslgram.additive_expression;

import bison;

auto additive_expression() {
  declare_sym(symbol_get("additive_expression"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("additive_expression"));
  grammar_current_rule_symbol_append(symbol_get("multiplicative_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("additive_expression"));
  grammar_current_rule_symbol_append(symbol_get("additive_expression"));
  grammar_current_rule_symbol_append(symbol_get("PLUS"));
  grammar_current_rule_symbol_append(symbol_get("multiplicative_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("additive_expression"));
  grammar_current_rule_symbol_append(symbol_get("additive_expression"));
  grammar_current_rule_symbol_append(symbol_get("DASH"));
  grammar_current_rule_symbol_append(symbol_get("multiplicative_expression"));
  grammar_current_rule_end();
}
