module glslgram.spirv_requirements_parameter;

import bison;

auto spirv_requirements_parameter() {
  declare_sym(symbol_get("spirv_requirements_parameter"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("spirv_requirements_parameter"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_symbol_append(symbol_get("EQUAL"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_BRACKET"));
  grammar_current_rule_symbol_append(symbol_get("spirv_extension_list"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_BRACKET"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("spirv_requirements_parameter"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_symbol_append(symbol_get("EQUAL"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_BRACKET"));
  grammar_current_rule_symbol_append(symbol_get("spirv_capability_list"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_BRACKET"));
  grammar_current_rule_end();
}
