module glslgram.parameter_declarator;

import bison;

auto parameter_declarator() {
  declare_sym(symbol_get("parameter_declarator"), symbol_class_.nterm_sym);
  
  grammar_current_rule_begin(symbol_get("parameter_declarator"));
  grammar_current_rule_symbol_append(symbol_get("type_specifier"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("parameter_declarator"));
  grammar_current_rule_symbol_append(symbol_get("type_specifier"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_symbol_append(symbol_get("array_specifier"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("parameter_declarator"));
  grammar_current_rule_symbol_append(symbol_get("type_specifier"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_symbol_append(symbol_get("EQUAL"));
  grammar_current_rule_symbol_append(symbol_get("initializer"));
  grammar_current_rule_end();
}
