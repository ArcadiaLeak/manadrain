module glslgram.single_declaration;

import bison;

auto single_declaration() {
  declare_sym(symbol_get("single_declaration"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("single_declaration"));
  grammar_current_rule_symbol_append(symbol_get("fully_specified_type"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("single_declaration"));
  grammar_current_rule_symbol_append(symbol_get("fully_specified_type"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("single_declaration"));
  grammar_current_rule_symbol_append(symbol_get("fully_specified_type"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_symbol_append(symbol_get("array_specifier"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("single_declaration"));
  grammar_current_rule_symbol_append(symbol_get("fully_specified_type"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_symbol_append(symbol_get("array_specifier"));
  grammar_current_rule_symbol_append(symbol_get("EQUAL"));
  grammar_current_rule_symbol_append(symbol_get("initializer"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("single_declaration"));
  grammar_current_rule_symbol_append(symbol_get("fully_specified_type"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_symbol_append(symbol_get("EQUAL"));
  grammar_current_rule_symbol_append(symbol_get("initializer"));
  grammar_current_rule_end();
}
