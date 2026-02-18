module glslgram.struct_declaration;

import bison;

auto struct_declaration() {
  declare_sym(symbol_get("struct_declaration"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("struct_declaration"));
  grammar_current_rule_symbol_append(symbol_get("type_specifier"));
  grammar_current_rule_symbol_append(symbol_get("struct_declarator_list"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("struct_declaration"));
  grammar_current_rule_symbol_append(symbol_get("type_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("type_specifier"));
  grammar_current_rule_symbol_append(symbol_get("struct_declarator_list"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();
}
