module glslgram.function_header_with_parameters;

import bison;

auto function_header_with_parameters() {
  declare_sym(symbol_get("function_header_with_parameters"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("function_header_with_parameters"));
  grammar_current_rule_symbol_append(symbol_get("function_header"));
  grammar_current_rule_symbol_append(symbol_get("parameter_declaration"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("function_header_with_parameters"));
  grammar_current_rule_symbol_append(symbol_get("function_header_with_parameters"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("parameter_declaration"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("function_header_with_parameters"));
  grammar_current_rule_symbol_append(symbol_get("function_header_with_parameters"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("DOT"));
  grammar_current_rule_symbol_append(symbol_get("DOT"));
  grammar_current_rule_symbol_append(symbol_get("DOT"));
  grammar_current_rule_end();
}
