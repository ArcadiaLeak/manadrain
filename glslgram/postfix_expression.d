module glslgram.postfix_expression;

import bison;

auto postfix_expression() {
  declare_sym(symbol_get("postfix_expression"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("postfix_expression"));
  grammar_current_rule_symbol_append(symbol_get("primary_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("postfix_expression"));
  grammar_current_rule_symbol_append(symbol_get("postfix_expression"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_BRACKET"));
  grammar_current_rule_symbol_append(symbol_get("integer_expression"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_BRACKET"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("postfix_expression"));
  grammar_current_rule_symbol_append(symbol_get("function_call"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("postfix_expression"));
  grammar_current_rule_symbol_append(symbol_get("postfix_expression"));
  grammar_current_rule_symbol_append(symbol_get("DOT"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("postfix_expression"));
  grammar_current_rule_symbol_append(symbol_get("postfix_expression"));
  grammar_current_rule_symbol_append(symbol_get("INC_OP"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("postfix_expression"));
  grammar_current_rule_symbol_append(symbol_get("postfix_expression"));
  grammar_current_rule_symbol_append(symbol_get("DEC_OP"));
  grammar_current_rule_end();
}