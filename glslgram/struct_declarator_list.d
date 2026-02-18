module glslgram.struct_declarator_list;

import bison;

auto struct_declarator_list() {
  declare_sym(symbol_get("struct_declarator_list"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("struct_declarator_list"));
  grammar_current_rule_symbol_append(symbol_get("struct_declarator"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("struct_declarator_list"));
  grammar_current_rule_symbol_append(symbol_get("struct_declarator_list"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("struct_declarator"));
  grammar_current_rule_end();
}
