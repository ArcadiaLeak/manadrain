module glslgram.type_parameter_specifier_list;

import bison;

auto type_parameter_specifier_list() {
  declare_sym(symbol_get("type_parameter_specifier_list"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("type_parameter_specifier_list"));
  grammar_current_rule_symbol_append(symbol_get("type_specifier"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("type_parameter_specifier_list"));
  grammar_current_rule_symbol_append(symbol_get("unary_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("type_parameter_specifier_list"));
  grammar_current_rule_symbol_append(symbol_get("type_parameter_specifier_list"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("unary_expression"));
  grammar_current_rule_end();
}
