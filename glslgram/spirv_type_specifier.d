module glslgram.spirv_type_specifier;

import bison;

auto spirv_type_specifier() {
  declare_sym(symbol_get("spirv_type_specifier"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("spirv_type_specifier"));
  grammar_current_rule_symbol_append(symbol_get("SPIRV_TYPE"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_PAREN"));
  grammar_current_rule_symbol_append(symbol_get("spirv_instruction_qualifier_list"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("spirv_type_parameter_list"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("spirv_type_specifier"));
  grammar_current_rule_symbol_append(symbol_get("SPIRV_TYPE"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_PAREN"));
  grammar_current_rule_symbol_append(symbol_get("spirv_requirements_list"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("spirv_instruction_qualifier_list"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("spirv_type_parameter_list"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("spirv_type_specifier"));
  grammar_current_rule_symbol_append(symbol_get("SPIRV_TYPE"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_PAREN"));
  grammar_current_rule_symbol_append(symbol_get("spirv_instruction_qualifier_list"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("spirv_type_specifier"));
  grammar_current_rule_symbol_append(symbol_get("SPIRV_TYPE"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_PAREN"));
  grammar_current_rule_symbol_append(symbol_get("spirv_requirements_list"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("spirv_instruction_qualifier_list"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_end();
}
