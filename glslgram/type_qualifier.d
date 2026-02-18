module glslgram.type_qualifier;

import bison;

auto type_qualifier() {
  declare_sym(symbol_get("type_qualifier"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("type_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("single_type_qualifier"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("type_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("type_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("single_type_qualifier"));
  grammar_current_rule_end();
}
