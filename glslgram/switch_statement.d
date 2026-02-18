module glslgram.switch_statement;

import bison;

auto switch_statement() {
  declare_sym(symbol_get("switch_statement"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("switch_statement"));
  grammar_current_rule_symbol_append(symbol_get("switch_statement_nonattributed"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("switch_statement"));
  grammar_current_rule_symbol_append(symbol_get("attribute"));
  grammar_current_rule_symbol_append(symbol_get("switch_statement_nonattributed"));
  grammar_current_rule_end();
}
