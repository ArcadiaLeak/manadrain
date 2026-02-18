module glslgram.spirv_type_parameter;

import bison;

auto spirv_type_parameter() {
  declare_sym(symbol_get("spirv_type_parameter"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("spirv_type_parameter"));
  grammar_current_rule_symbol_append(symbol_get("constant_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("spirv_type_parameter"));
  grammar_current_rule_symbol_append(symbol_get("type_specifier_nonarray"));
  grammar_current_rule_end();
}
