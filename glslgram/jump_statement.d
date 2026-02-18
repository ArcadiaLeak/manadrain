module glslgram.jump_statement;

import bison;

auto jump_statement() {
  declare_sym(symbol_get("jump_statement"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("jump_statement"));
  grammar_current_rule_symbol_append(symbol_get("CONTINUE"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("jump_statement"));
  grammar_current_rule_symbol_append(symbol_get("BREAK"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("jump_statement"));
  grammar_current_rule_symbol_append(symbol_get("RETURN"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("jump_statement"));
  grammar_current_rule_symbol_append(symbol_get("RETURN"));
  grammar_current_rule_symbol_append(symbol_get("expression"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("jump_statement"));
  grammar_current_rule_symbol_append(symbol_get("DISCARD"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("jump_statement"));
  grammar_current_rule_symbol_append(symbol_get("TERMINATE_INVOCATION"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("jump_statement"));
  grammar_current_rule_symbol_append(symbol_get("TERMINATE_RAY"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("jump_statement"));
  grammar_current_rule_symbol_append(symbol_get("IGNORE_INTERSECTION"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();
}
