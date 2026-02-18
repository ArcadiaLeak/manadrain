module glslgram.initializer;

import bison;

auto initializer() {
  declare_sym(symbol_get("initializer"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("initializer"));
  grammar_current_rule_symbol_append(symbol_get("assignment_expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("initializer"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_BRACE"));
  grammar_current_rule_symbol_append(symbol_get("initializer_list"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_BRACE"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("initializer"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_BRACE"));
  grammar_current_rule_symbol_append(symbol_get("initializer_list"));
  grammar_current_rule_symbol_append(symbol_get("COMMA"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_BRACE"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("initializer"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_BRACE"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_BRACE"));
  grammar_current_rule_end();
}
