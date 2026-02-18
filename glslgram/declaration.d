module glslgram.declaration;

import bison;

auto declaration() {
  declare_sym(symbol_get("declaration"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("declaration"));
  grammar_current_rule_symbol_append(symbol_get("function_prototype"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("declaration"));
  grammar_current_rule_symbol_append(symbol_get("spirv_instruction_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("function_prototype"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("declaration"));
  grammar_current_rule_symbol_append(symbol_get("spirv_execution_mode_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("declaration"));
  grammar_current_rule_symbol_append(symbol_get("init_declarator_list"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("declaration"));
  grammar_current_rule_symbol_append(symbol_get("PRECISION"));
  grammar_current_rule_symbol_append(symbol_get("precision_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("type_specifier"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("declaration"));
  grammar_current_rule_symbol_append(symbol_get("block_structure"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("declaration"));
  grammar_current_rule_symbol_append(symbol_get("block_structure"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("declaration"));
  grammar_current_rule_symbol_append(symbol_get("block_structure"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_symbol_append(symbol_get("array_specifier"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("declaration"));
  grammar_current_rule_symbol_append(symbol_get("type_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("declaration"));
  grammar_current_rule_symbol_append(symbol_get("type_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("identifier_list"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();
}
