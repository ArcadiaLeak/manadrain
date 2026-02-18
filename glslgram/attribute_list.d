module glslgram.attribute_list;

import bison;

auto attribute_list() {
  declare_sym(symbol_get("attribute_list"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("attribute_list"));
  grammar_current_rule_symbol_append(symbol_get("single_attribute"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("attribute_list"));
  grammar_current_rule_symbol_append(symbol_get("attribute_list"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("single_attribute"));
  grammar_current_rule_end();
}
