module glslgram.init_declarator_list;

import bison;

auto init_declarator_list() {
  declare_sym(symbol_get("init_declarator_list"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("init_declarator_list"));
  grammar_current_rule_symbol_append(symbol_get("single_declaration"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("init_declarator_list"));
  grammar_current_rule_symbol_append(symbol_get("init_declarator_list"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("init_declarator_list"));
  grammar_current_rule_symbol_append(symbol_get("init_declarator_list"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_symbol_append(symbol_get("array_specifier"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("init_declarator_list"));
  grammar_current_rule_symbol_append(symbol_get("init_declarator_list"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_symbol_append(symbol_get("array_specifier"));
  grammar_current_rule_symbol_append(symbol_get("EQUAL"));
  grammar_current_rule_symbol_append(symbol_get("initializer"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("init_declarator_list"));
  grammar_current_rule_symbol_append(symbol_get("init_declarator_list"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_symbol_append(symbol_get("EQUAL"));
  grammar_current_rule_symbol_append(symbol_get("initializer"));
  grammar_current_rule_end();
}
