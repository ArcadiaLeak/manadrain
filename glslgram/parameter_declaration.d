module glslgram.parameter_declaration;

import bison;

auto parameter_declaration() {
  declare_sym(symbol_get("parameter_declaration"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("parameter_declaration"));
  grammar_current_rule_symbol_append(symbol_get("type_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("parameter_declarator"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("parameter_declaration"));
  grammar_current_rule_symbol_append(symbol_get("parameter_declarator"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("parameter_declaration"));
  grammar_current_rule_symbol_append(symbol_get("type_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("parameter_type_specifier"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("parameter_declaration"));
  grammar_current_rule_symbol_append(symbol_get("parameter_type_specifier"));
  grammar_current_rule_end();
}
