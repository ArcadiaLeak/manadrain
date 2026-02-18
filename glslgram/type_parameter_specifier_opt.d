module glslgram.type_parameter_specifier_opt;

import bison;

auto type_parameter_specifier_opt() {
  declare_sym(symbol_get("type_parameter_specifier_opt"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("type_parameter_specifier_opt"));
  grammar_current_rule_symbol_append(symbol_get("type_parameter_specifier"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("type_parameter_specifier_opt"));
  grammar_current_rule_end();
}
