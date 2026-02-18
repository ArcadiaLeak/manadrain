module glslgram.for_init_statement;

import bison;

auto for_init_statement() {
  declare_sym(symbol_get("for_init_statement"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("for_init_statement"));
  grammar_current_rule_symbol_append(symbol_get("expression_statement"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("for_init_statement"));
  grammar_current_rule_symbol_append(symbol_get("declaration_statement"));
  grammar_current_rule_end();
}
