module glslgram.layout_qualifier_id;

import bison;

auto layout_qualifier_id() {
  declare_sym(symbol_get("layout_qualifier_id"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("layout_qualifier_id"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("layout_qualifier_id"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_symbol_append(symbol_get("EQUAL"));
  grammar_current_rule_symbol_append(symbol_get("constant_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("layout_qualifier_id"));
  grammar_current_rule_symbol_append(symbol_get("SHARED"));
  grammar_current_rule_end();
}
