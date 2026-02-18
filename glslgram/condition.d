module glslgram.condition;

import bison;

auto condition() {
  declare_sym(symbol_get("condition"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("condition"));
  grammar_current_rule_symbol_append(symbol_get("expression"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("condition"));
  grammar_current_rule_symbol_append(symbol_get("fully_specified_type"));
  grammar_current_rule_symbol_append(symbol_get("IDENTIFIER"));
  grammar_current_rule_symbol_append(symbol_get("EQUAL"));
  grammar_current_rule_symbol_append(symbol_get("initializer"));
  grammar_current_rule_end();
}
