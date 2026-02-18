module glslgram.external_declaration;

import bison;

auto external_declaration() {
  declare_sym(symbol_get("external_declaration"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("external_declaration"));
  grammar_current_rule_symbol_append(symbol_get("function_definition"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("external_declaration"));
  grammar_current_rule_symbol_append(symbol_get("declaration"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("external_declaration"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();
}
