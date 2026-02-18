module glslgram.selection_statement;

import bison;

auto selection_statement() {
  declare_sym(symbol_get("selection_statement"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("selection_statement"));
  grammar_current_rule_symbol_append(symbol_get("selection_statement_nonattributed"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("selection_statement"));
  grammar_current_rule_symbol_append(symbol_get("attribute"));
  grammar_current_rule_symbol_append(symbol_get("selection_statement_nonattributed"));
  grammar_current_rule_end();
}
