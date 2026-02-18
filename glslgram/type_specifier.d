module glslgram.type_specifier;

import bison;

auto type_specifier() {
  declare_sym(symbol_get("type_specifier"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("type_specifier"));
  grammar_current_rule_symbol_append(symbol_get("type_specifier_nonarray"));
  grammar_current_rule_symbol_append(symbol_get("type_parameter_specifier_opt"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("type_specifier"));
  grammar_current_rule_symbol_append(symbol_get("type_specifier_nonarray"));
  grammar_current_rule_symbol_append(symbol_get("type_parameter_specifier_opt"));
  grammar_current_rule_symbol_append(symbol_get("array_specifier"));
  grammar_current_rule_end();
}
