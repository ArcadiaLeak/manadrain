module glslgram.precision_qualifier;

import bison;

auto precision_qualifier() {
  declare_sym(symbol_get("precision_qualifier"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("precision_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("HIGH_PRECISION"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("precision_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("MEDIUM_PRECISION"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("precision_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("LOW_PRECISION"));
  grammar_current_rule_end();
}
