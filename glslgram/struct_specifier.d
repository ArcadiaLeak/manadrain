module glslgram.struct_specifier;

import bison;

auto struct_specifier() {
  declare_sym(symbol_get("struct_specifier"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("struct_specifier"));
  grammar_current_rule_symbol_append(symbol_get("STRUCT"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_BRACE"));
  grammar_midrule_action();
  grammar_current_rule_symbol_append(symbol_get("struct_declaration_list"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_BRACE"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("struct_specifier"));
  grammar_current_rule_symbol_append(symbol_get("STRUCT"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_BRACE"));
  grammar_midrule_action();
  grammar_current_rule_symbol_append(symbol_get("struct_declaration_list"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_BRACE"));
  grammar_current_rule_end();
}
