module glslgram.spirv_decorate_parameter;

import bison;

auto spirv_decorate_parameter() {
  declare_sym(symbol_get("spirv_decorate_parameter"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("spirv_decorate_parameter"));
  grammar_current_rule_symbol_append(symbol_get("FLOATCONSTANT"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("spirv_decorate_parameter"));
  grammar_current_rule_symbol_append(symbol_get("INTCONSTANT"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("spirv_decorate_parameter"));
  grammar_current_rule_symbol_append(symbol_get("UINTCONSTANT"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("spirv_decorate_parameter"));
  grammar_current_rule_symbol_append(symbol_get("BOOLCONSTANT"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("spirv_decorate_id_parameter_list"));
  grammar_current_rule_symbol_append(symbol_get("spirv_decorate_id_parameter"));
  grammar_current_rule_end();
}
