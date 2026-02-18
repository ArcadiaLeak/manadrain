module glslgram.translation_unit;

import bison;

auto translation_unit() {
  declare_sym(symbol_get("translation_unit"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("translation_unit"));
  grammar_current_rule_symbol_append(symbol_get("external_declaration"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("translation_unit"));
  grammar_current_rule_symbol_append(symbol_get("translation_unit"));
  grammar_current_rule_symbol_append(symbol_get("external_declaration"));
  grammar_current_rule_end();
}
