module glslgram.type_name_list;

import bison;

auto type_name_list() {
  declare_sym(symbol_get("type_name_list"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("type_name_list"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("type_name_list"));
  grammar_current_rule_symbol_append(symbol_get("type_name_list"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_end();
}
