module glslgram.spirv_execution_mode_qualifier;

import bison;

auto spirv_execution_mode_qualifier() {
  declare_sym(symbol_get("spirv_execution_mode_qualifier"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("spirv_execution_mode_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("SPIRV_EXECUTION_MODE"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_PAREN"));
  grammar_current_rule_symbol_append(symbol_get("INTCONSTANT"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("spirv_execution_mode_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("SPIRV_EXECUTION_MODE"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_PAREN"));
  grammar_current_rule_symbol_append(symbol_get("spirv_requirements_list"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("INTCONSTANT"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("spirv_execution_mode_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("SPIRV_EXECUTION_MODE"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_PAREN"));
  grammar_current_rule_symbol_append(symbol_get("INTCONSTANT"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("spirv_execution_mode_parameter_list"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("spirv_execution_mode_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("SPIRV_EXECUTION_MODE"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_PAREN"));
  grammar_current_rule_symbol_append(symbol_get("spirv_requirements_list"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("INTCONSTANT"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("spirv_execution_mode_parameter_list"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("spirv_execution_mode_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("SPIRV_EXECUTION_MODE_ID"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_PAREN"));
  grammar_current_rule_symbol_append(symbol_get("INTCONSTANT"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("spirv_execution_mode_id_parameter_list"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("spirv_execution_mode_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("SPIRV_EXECUTION_MODE_ID"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_PAREN"));
  grammar_current_rule_symbol_append(symbol_get("spirv_requirements_list"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("INTCONSTANT"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("spirv_execution_mode_id_parameter_list"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_end();
}
