module glslgram.function_call_generic;

import bison;

auto function_call_generic() {
  declare_sym(symbol_get("function_call_generic"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("function_call_generic"));
  grammar_current_rule_symbol_append(symbol_get("function_call_header_with_parameters"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("function_call_generic"));
  grammar_current_rule_symbol_append(symbol_get("function_call_header_no_parameters"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_end();
}
