module glslgram.spirv_capability_list;

import bison;

auto spirv_capability_list() {
  declare_sym(symbol_get("spirv_capability_list"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("spirv_capability_list"));
  grammar_current_rule_symbol_append(symbol_get("INTCONSTANT"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("spirv_capability_list"));
  grammar_current_rule_symbol_append(symbol_get("spirv_capability_list"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("INTCONSTANT"));
  grammar_current_rule_end();
}
