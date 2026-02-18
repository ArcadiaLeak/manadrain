module glslgram.statement_scoped;

import bison;

auto statement_scoped() {
  declare_sym(symbol_get("statement_scoped"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("statement_scoped"));
  grammar_midrule_action();
  grammar_current_rule_symbol_append(symbol_get("compound_statement"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("statement_scoped"));
  grammar_midrule_action();
  grammar_current_rule_symbol_append(symbol_get("simple_statement"));
  grammar_current_rule_end();
}
