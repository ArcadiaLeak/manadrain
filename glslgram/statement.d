module glslgram.statement;

import bison;

auto statement() {
  declare_sym(symbol_get("statement"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("statement"));
  grammar_current_rule_symbol_append(symbol_get("compound_statement"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("statement"));
  grammar_current_rule_symbol_append(symbol_get("simple_statement"));
  grammar_current_rule_end();
}
