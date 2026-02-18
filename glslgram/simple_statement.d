module glslgram.simple_statement;

import bison;

auto simple_statement() {
  declare_sym(symbol_get("simple_statement"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("simple_statement"));
  grammar_current_rule_symbol_append(symbol_get("declaration_statement"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("simple_statement"));
  grammar_current_rule_symbol_append(symbol_get("expression_statement"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("simple_statement"));
  grammar_current_rule_symbol_append(symbol_get("selection_statement"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("simple_statement"));
  grammar_current_rule_symbol_append(symbol_get("switch_statement"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("simple_statement"));
  grammar_current_rule_symbol_append(symbol_get("case_label"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("simple_statement"));
  grammar_current_rule_symbol_append(symbol_get("iteration_statement"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("simple_statement"));
  grammar_current_rule_symbol_append(symbol_get("jump_statement"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("simple_statement"));
  grammar_current_rule_symbol_append(symbol_get("demote_statement"));
  grammar_current_rule_end();
}
