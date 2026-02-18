module glslgram.function_identifier;

import bison;

auto function_identifier() {
  declare_sym(symbol_get("function_identifier"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("function_identifier"));
  grammar_current_rule_symbol_append(symbol_get("type_specifier"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("function_identifier"));
  grammar_current_rule_symbol_append(symbol_get("postfix_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("function_identifier"));
  grammar_current_rule_symbol_append(symbol_get("non_uniform_qualifier"));
  grammar_current_rule_end();
}
