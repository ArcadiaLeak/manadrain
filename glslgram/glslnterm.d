module glslgram.glslnterm;

import bison;

import glslgram.additive_expression;
import glslgram.and_expression;
import glslgram.array_specifier;
import glslgram.assignment_expression;
import glslgram.assignment_operator;
import glslgram.attribute_list;
import glslgram.case_label;
import glslgram.compound_statement_no_new_scope;
import glslgram.compound_statement;
import glslgram.condition;
import glslgram.conditional_expression;
import glslgram.conditionopt;
import glslgram.declaration;
import glslgram.equality_expression;
import glslgram.exclusive_or_expression;
import glslgram.expression_statement;
import glslgram.expression;
import glslgram.external_declaration;
import glslgram.for_init_statement;
import glslgram.for_rest_statement;
import glslgram.fully_specified_type;
import glslgram.function_call_generic;
import glslgram.function_call_header_no_parameters;
import glslgram.function_call_header_with_parameters;
import glslgram.function_declarator;
import glslgram.function_header_with_parameters;
import glslgram.function_identifier;
import glslgram.function_prototype;
import glslgram.identifier_list;
import glslgram.inclusive_or_expression;
import glslgram.init_declarator_list;
import glslgram.initializer_list;
import glslgram.initializer;
import glslgram.interpolation_qualifier;
import glslgram.iteration_statement_nonattributed;
import glslgram.iteration_statement;
import glslgram.jump_statement;
import glslgram.layout_qualifier_id_list;
import glslgram.layout_qualifier_id;
import glslgram.logical_and_expression;
import glslgram.logical_or_expression;
import glslgram.logical_xor_expression;
import glslgram.multiplicative_expression;
import glslgram.parameter_declaration;
import glslgram.parameter_declarator;
import glslgram.postfix_expression;
import glslgram.precision_qualifier;
import glslgram.primary_expression;
import glslgram.relational_expression;
import glslgram.selection_rest_statement;
import glslgram.selection_statement;
import glslgram.shift_expression;
import glslgram.simple_statement;
import glslgram.single_attribute;
import glslgram.single_declaration;
import glslgram.single_type_qualifier;
import glslgram.spirv_capability_list;
import glslgram.spirv_decorate_id_parameter;
import glslgram.spirv_decorate_parameter_list;
import glslgram.spirv_decorate_parameter;
import glslgram.spirv_decorate_qualifier;
import glslgram.spirv_decorate_string_parameter_list;
import glslgram.spirv_execution_mode_id_parameter_list;
import glslgram.spirv_execution_mode_parameter_list;
import glslgram.spirv_execution_mode_parameter;
import glslgram.spirv_execution_mode_qualifier;
import glslgram.spirv_extension_list;
import glslgram.spirv_instruction_qualifier_id;
import glslgram.spirv_instruction_qualifier_list;
import glslgram.spirv_instruction_qualifier;
import glslgram.spirv_requirements_list;
import glslgram.spirv_requirements_parameter;
import glslgram.spirv_storage_class_qualifier;
import glslgram.spirv_type_parameter_list;
import glslgram.spirv_type_parameter;
import glslgram.spirv_type_specifier;
import glslgram.statement_list;
import glslgram.statement_no_new_scope;
import glslgram.statement_scoped;
import glslgram.statement;
import glslgram.storage_qualifier;
import glslgram.struct_declaration_list;
import glslgram.struct_declaration;
import glslgram.struct_declarator_list;
import glslgram.struct_declarator;
import glslgram.struct_specifier;
import glslgram.switch_statement_list;
import glslgram.switch_statement;
import glslgram.translation_unit;
import glslgram.type_name_list;
import glslgram.type_parameter_specifier_list;
import glslgram.type_parameter_specifier_opt;
import glslgram.type_qualifier;
import glslgram.type_specifier_nonarray;
import glslgram.type_specifier;
import glslgram.unary_expression;
import glslgram.unary_operator;

auto glslnterm() {
  grammar_start_symbols_add(new symbol_list(symbol_get("translation_unit")));

  declare_sym(symbol_get("variable_identifier"), symbol_class_.nterm_sym);
  grammar_current_rule_begin(symbol_get("variable_identifier"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_end();

  primary_expression();
  postfix_expression();

  declare_sym(symbol_get("integer_expression"), symbol_class_.nterm_sym);
  grammar_current_rule_begin(symbol_get("integer_expression"));
  grammar_current_rule_symbol_append(symbol_get("expression"));
  grammar_current_rule_end();

  declare_sym(symbol_get("function_call"), symbol_class_.nterm_sym);
  grammar_current_rule_begin(symbol_get("function_call"));
  grammar_current_rule_symbol_append(symbol_get("function_call_or_method"));
  grammar_current_rule_end();

  declare_sym(symbol_get("function_call_or_method"), symbol_class_.nterm_sym);
  grammar_current_rule_begin(symbol_get("function_call_or_method"));
  grammar_current_rule_symbol_append(symbol_get("function_call_generic"));
  grammar_current_rule_end();

  function_call_generic();
  function_call_header_no_parameters();
  function_call_header_with_parameters();

  declare_sym(symbol_get("function_call_header"), symbol_class_.nterm_sym);
  grammar_current_rule_begin(symbol_get("function_call_header"));
  grammar_current_rule_symbol_append(symbol_get("function_identifier"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_PAREN"));
  grammar_current_rule_end();

  function_identifier();
  unary_expression();
  unary_operator();
  multiplicative_expression();
  additive_expression();
  shift_expression();
  relational_expression();
  equality_expression();
  and_expression();
  exclusive_or_expression();
  inclusive_or_expression();
  logical_and_expression();
  logical_xor_expression();
  logical_or_expression();
  conditional_expression();
  assignment_expression();
  assignment_operator();
  expression();

  declare_sym(symbol_get("constant_expression"), symbol_class_.nterm_sym);
  grammar_current_rule_begin(symbol_get("constant_expression"));
  grammar_current_rule_symbol_append(symbol_get("conditional_expression"));
  grammar_current_rule_end();

  declaration();

  declare_sym(symbol_get("block_structure"), symbol_class_.nterm_sym);
  grammar_current_rule_begin(symbol_get("block_structure"));
  grammar_current_rule_symbol_append(symbol_get("type_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_BRACE"));
  grammar_midrule_action();
  grammar_current_rule_symbol_append(symbol_get("struct_declaration_list"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_BRACE"));
  grammar_current_rule_end();

  identifier_list();
  function_prototype();
  function_declarator();
  function_header_with_parameters();

  declare_sym(symbol_get("function_header"), symbol_class_.nterm_sym);
  grammar_current_rule_begin(symbol_get("function_header"));
  grammar_current_rule_symbol_append(symbol_get("fully_specified_type"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_PAREN"));
  grammar_current_rule_end();

  parameter_declarator();
  parameter_declaration();

  declare_sym(symbol_get("parameter_type_specifier"), symbol_class_.nterm_sym);
  grammar_current_rule_begin(symbol_get("parameter_type_specifier"));
  grammar_current_rule_symbol_append(symbol_get("type_specifier"));
  grammar_current_rule_end();

  init_declarator_list();
  single_declaration();
  fully_specified_type();

  declare_sym(symbol_get("invariant_qualifier"), symbol_class_.nterm_sym);
  grammar_current_rule_begin(symbol_get("invariant_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("INVARIANT"));
  grammar_current_rule_end();

  interpolation_qualifier();

  declare_sym(symbol_get("layout_qualifier"), symbol_class_.nterm_sym);
  grammar_current_rule_begin(symbol_get("layout_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("LAYOUT"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_PAREN"));
  grammar_current_rule_symbol_append(symbol_get("layout_qualifier_id_list"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_end();

  layout_qualifier_id_list();
  layout_qualifier_id();

  declare_sym(symbol_get("precise_qualifier"), symbol_class_.nterm_sym);
  grammar_current_rule_begin(symbol_get("precise_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("PRECISE"));
  grammar_current_rule_end();

  type_qualifier();
  single_type_qualifier();
  storage_qualifier();

  declare_sym(symbol_get("non_uniform_qualifier"), symbol_class_.nterm_sym);
  grammar_current_rule_begin(symbol_get("non_uniform_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("NONUNIFORM"));
  grammar_current_rule_end();

  type_name_list();
  type_specifier();
  array_specifier();
  type_parameter_specifier_opt();

  declare_sym(symbol_get("type_parameter_specifier"), symbol_class_.nterm_sym);
  grammar_current_rule_begin(symbol_get("type_parameter_specifier"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_ANGLE"));
  grammar_current_rule_symbol_append(symbol_get("type_parameter_specifier_list"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_ANGLE"));
  grammar_current_rule_end();

  type_parameter_specifier_list();
  type_specifier_nonarray();
  precision_qualifier();
  struct_specifier();
  struct_declaration_list();
  struct_declaration();
  struct_declarator_list();
  struct_declarator();
  initializer();
  initializer_list();

  declare_sym(symbol_get("declaration_statement"), symbol_class_.nterm_sym);
  grammar_current_rule_begin(symbol_get("declaration_statement"));
  grammar_current_rule_symbol_append(symbol_get("declaration"));
  grammar_current_rule_end();

  statement();
  simple_statement();

  declare_sym(symbol_get("demote_statement"), symbol_class_.nterm_sym);
  grammar_current_rule_begin(symbol_get("demote_statement"));
  grammar_current_rule_symbol_append(symbol_get("DEMOTE"));
  grammar_current_rule_symbol_append(symbol_get("SEMICOLON"));
  grammar_current_rule_end();

  compound_statement();
  statement_no_new_scope();
  statement_scoped();
  compound_statement_no_new_scope();
  statement_list();
  expression_statement();
  selection_statement();

  declare_sym(symbol_get("selection_statement_nonattributed"), symbol_class_.nterm_sym);
  grammar_current_rule_begin(symbol_get("selection_statement_nonattributed"));
  grammar_current_rule_symbol_append(symbol_get("IF"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_PAREN"));
  grammar_current_rule_symbol_append(symbol_get("expression"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_symbol_append(symbol_get("selection_rest_statement"));
  grammar_current_rule_end();

  selection_rest_statement();
  condition();
  switch_statement();

  declare_sym(symbol_get("switch_statement_nonattributed"), symbol_class_.nterm_sym);
  grammar_current_rule_begin(symbol_get("switch_statement_nonattributed"));
  grammar_current_rule_symbol_append(symbol_get("SWITCH"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_PAREN"));
  grammar_current_rule_symbol_append(symbol_get("expression"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_midrule_action();
  grammar_current_rule_symbol_append(symbol_get("LEFT_BRACE"));
  grammar_current_rule_symbol_append(symbol_get("switch_statement_list"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_BRACE"));
  grammar_current_rule_end();

  switch_statement_list();
  case_label();
  iteration_statement();
  iteration_statement_nonattributed();
  for_init_statement();
  conditionopt();
  for_rest_statement();
  jump_statement();
  translation_unit();
  external_declaration();
  
  declare_sym(symbol_get("function_definition"), symbol_class_.nterm_sym);
  grammar_current_rule_begin(symbol_get("function_definition"));
  grammar_current_rule_symbol_append(symbol_get("function_prototype"));
  grammar_midrule_action();
  grammar_current_rule_symbol_append(symbol_get("compound_statement_no_new_scope"));
  grammar_current_rule_end();

  declare_sym(symbol_get("attribute"), symbol_class_.nterm_sym);
  grammar_current_rule_begin(symbol_get("attribute"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_BRACKET"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_BRACKET"));
  grammar_current_rule_symbol_append(symbol_get("attribute_list"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_BRACKET"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_BRACKET"));
  grammar_current_rule_end();

  attribute_list();
  single_attribute();
  spirv_requirements_list();
  spirv_requirements_parameter();
  spirv_extension_list();
  spirv_capability_list();
  spirv_execution_mode_qualifier();
  spirv_execution_mode_parameter_list();
  spirv_execution_mode_parameter();
  spirv_execution_mode_id_parameter_list();
  spirv_storage_class_qualifier();
  spirv_decorate_qualifier();
  spirv_decorate_parameter_list();
  spirv_decorate_parameter();
  
  declare_sym(symbol_get("spirv_decorate_id_parameter_list"), symbol_class_.nterm_sym);
  grammar_current_rule_begin(symbol_get("spirv_decorate_id_parameter_list"));
  grammar_current_rule_symbol_append(symbol_get("spirv_decorate_id_parameter_list"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("spirv_decorate_id_parameter"));
  grammar_current_rule_end();

  spirv_decorate_id_parameter();
  spirv_decorate_string_parameter_list();
  spirv_type_specifier();
  spirv_type_parameter_list();
  spirv_type_parameter();
  spirv_instruction_qualifier();
  spirv_instruction_qualifier_list();
  spirv_instruction_qualifier_id();
}
