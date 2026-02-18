module glslgram.function_call_header_with_parameters;

import bison;

auto function_call_header_with_parameters() {
  declare_sym(symbol_get("function_call_header_with_parameters"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("function_call_header_with_parameters"));
  grammar_current_rule_symbol_append(symbol_get("function_call_header"));
  grammar_current_rule_symbol_append(symbol_get("assignment_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("function_call_header_with_parameters"));
  grammar_current_rule_symbol_append(symbol_get("function_call_header_with_parameters"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("assignment_expression"));
  grammar_current_rule_end();
}
