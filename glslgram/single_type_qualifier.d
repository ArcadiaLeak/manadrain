module glslgram.single_type_qualifier;

import bison;

auto single_type_qualifier() {
  declare_sym(symbol_get("single_type_qualifier"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("single_type_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("storage_qualifier"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("single_type_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("layout_qualifier"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("single_type_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("precision_qualifier"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("single_type_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("interpolation_qualifier"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("single_type_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("invariant_qualifier"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("single_type_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("precise_qualifier"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("single_type_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("non_uniform_qualifier"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("single_type_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("spirv_storage_class_qualifier"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("single_type_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("spirv_decorate_qualifier"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("single_type_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("SPIRV_BY_REFERENCE"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("single_type_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("SPIRV_LITERAL"));
  grammar_current_rule_end();
}
