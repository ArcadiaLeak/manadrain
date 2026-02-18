module glslgram.struct_declaration_list;

import bison;

auto struct_declaration_list() {
  declare_sym(symbol_get("struct_declaration_list"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("struct_declaration_list"));
  grammar_current_rule_symbol_append(symbol_get("struct_declaration"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("struct_declaration_list"));
  grammar_current_rule_symbol_append(symbol_get("struct_declaration_list"));
  grammar_current_rule_symbol_append(symbol_get("struct_declaration"));
  grammar_current_rule_end();
}
