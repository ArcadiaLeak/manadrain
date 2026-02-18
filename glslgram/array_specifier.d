module glslgram.array_specifier;

import bison;

auto array_specifier() {
  declare_sym(symbol_get("array_specifier"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("array_specifier"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_BRACKET"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_BRACKET"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("array_specifier"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_BRACKET"));
  grammar_current_rule_symbol_append(symbol_get("conditional_expression"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_BRACKET"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("array_specifier"));
  grammar_current_rule_symbol_append(symbol_get("array_specifier"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_BRACKET"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_BRACKET"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("array_specifier"));
  grammar_current_rule_symbol_append(symbol_get("array_specifier"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_BRACKET"));
  grammar_current_rule_symbol_append(symbol_get("conditional_expression"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_BRACKET"));
  grammar_current_rule_end();
}
