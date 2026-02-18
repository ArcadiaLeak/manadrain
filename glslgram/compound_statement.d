module glslgram.compound_statement;

import bison;

auto compound_statement() {
  declare_sym(symbol_get("compound_statement"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("compound_statement"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_BRACE"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_BRACE"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("compound_statement"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_BRACE"));
  grammar_midrule_action();
  grammar_current_rule_symbol_append(symbol_get("statement_list"));
  grammar_midrule_action();
  grammar_current_rule_symbol_append(symbol_get("RIGHT_BRACE"));
  grammar_current_rule_end();
}
