module glslgram.spirv_instruction_qualifier_id;

import bison;

void spirv_instruction_qualifier_id() {
  declare_sym(symbol_get("spirv_instruction_qualifier_id"), symbol_class_.nterm_sym);
  
  grammar_current_rule_begin(symbol_get("spirv_instruction_qualifier_id"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_symbol_append(symbol_get("EQUAL"));
  grammar_current_rule_symbol_append(symbol_get("STRING_LITERAL"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("spirv_instruction_qualifier_id"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_symbol_append(symbol_get("EQUAL"));
  grammar_current_rule_symbol_append(symbol_get("INTCONSTANT"));
  grammar_current_rule_end();
}
