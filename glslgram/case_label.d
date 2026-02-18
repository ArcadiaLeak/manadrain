module glslgram.case_label;

import bison;

auto case_label() {
  declare_sym(symbol_get("case_label"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("case_label"));
  grammar_current_rule_symbol_append(symbol_get("CASE"));
  grammar_current_rule_symbol_append(symbol_get("expression"));
  grammar_current_rule_symbol_append(symbol_get("COLON"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("case_label"));
  grammar_current_rule_symbol_append(symbol_get("DEFAULT"));
  grammar_current_rule_symbol_append(symbol_get("COLON"));
  grammar_current_rule_end();
}
