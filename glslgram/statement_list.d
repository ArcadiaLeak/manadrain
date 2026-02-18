module glslgram.statement_list;

import bison;

auto statement_list() {
  declare_sym(symbol_get("statement_list"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("statement_list"));
  grammar_current_rule_symbol_append(symbol_get("statement"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("statement_list"));
  grammar_current_rule_symbol_append(symbol_get("statement_list"));
  grammar_current_rule_symbol_append(symbol_get("statement"));
  grammar_current_rule_end();
}
