module glslgram.function_declarator;

import bison;

auto function_declarator() {
  declare_sym(symbol_get("function_declarator"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("function_declarator"));
  grammar_current_rule_symbol_append(symbol_get("function_header"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("function_declarator"));
  grammar_current_rule_symbol_append(symbol_get("function_header_with_parameters"));
  grammar_current_rule_end();
}
