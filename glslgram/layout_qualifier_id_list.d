module glslgram.layout_qualifier_id_list;

import bison;

auto layout_qualifier_id_list() {
  declare_sym(symbol_get("layout_qualifier_id_list"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("layout_qualifier_id_list"));
  grammar_current_rule_symbol_append(symbol_get("layout_qualifier_id"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("layout_qualifier_id_list"));
  grammar_current_rule_symbol_append(symbol_get("layout_qualifier_id_list"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("layout_qualifier_id"));
  grammar_current_rule_end();
}
