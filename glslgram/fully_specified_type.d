module glslgram.fully_specified_type;

import bison;

auto fully_specified_type() {
  declare_sym(symbol_get("fully_specified_type"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("fully_specified_type"));
  grammar_current_rule_symbol_append(symbol_get("type_specifier"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("fully_specified_type"));
  grammar_current_rule_symbol_append(symbol_get("type_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("type_specifier"));
  grammar_current_rule_end();
}
