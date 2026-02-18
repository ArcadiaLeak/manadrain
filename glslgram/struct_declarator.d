module glslgram.struct_declarator;

import bison;

auto struct_declarator() {
  declare_sym(symbol_get("struct_declarator"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("struct_declarator"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("struct_declarator"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_symbol_append(symbol_get("array_specifier"));
  grammar_current_rule_end();
}
