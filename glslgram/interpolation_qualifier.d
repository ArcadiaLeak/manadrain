module glslgram.interpolation_qualifier;

import bison;

void interpolation_qualifier() {
  declare_sym(symbol_get("interpolation_qualifier"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("interpolation_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("SMOOTH"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("interpolation_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("FLAT"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("interpolation_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("NOPERSPECTIVE"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("interpolation_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("EXPLICITINTERPAMD"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("interpolation_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("PERVERTEXNV"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("interpolation_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("PERVERTEXEXT"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("interpolation_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("PERPRIMITIVENV"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("interpolation_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("PERPRIMITIVEEXT"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("interpolation_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("PERVIEWNV"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("interpolation_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("PERTASKNV"));
  grammar_current_rule_end();
}
