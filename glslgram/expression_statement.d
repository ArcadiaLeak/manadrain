module glslgram.expression_statement;

import bison;

auto expression_statement() {
  declare_sym(symbol_get("expression_statement"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("expression_statement"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("expression_statement"));
  grammar_current_rule_symbol_append(symbol_get("expression"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();
}
