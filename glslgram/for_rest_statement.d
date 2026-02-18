module glslgram.for_rest_statement;

import bison;

auto for_rest_statement() {
  declare_sym(symbol_get("for_rest_statement"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("for_rest_statement"));
  grammar_current_rule_symbol_append(symbol_get("conditionopt"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("for_rest_statement"));
  grammar_current_rule_symbol_append(symbol_get("conditionopt"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_symbol_append(symbol_get("expression"));
  grammar_current_rule_end();
}
