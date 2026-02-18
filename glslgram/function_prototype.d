module glslgram.function_prototype;

import bison;

auto function_prototype() {
  declare_sym(symbol_get("function_prototype"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("function_prototype"));
  grammar_current_rule_symbol_append(symbol_get("function_declarator"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("function_prototype"));
  grammar_current_rule_symbol_append(symbol_get("function_declarator"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_symbol_append(symbol_get("attribute"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("function_prototype"));
  grammar_current_rule_symbol_append(symbol_get("attribute"));
  grammar_current_rule_symbol_append(symbol_get("function_declarator"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("function_prototype"));
  grammar_current_rule_symbol_append(symbol_get("attribute"));
  grammar_current_rule_symbol_append(symbol_get("function_declarator"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_symbol_append(symbol_get("attribute"));
  grammar_current_rule_end();
}
