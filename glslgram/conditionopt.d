module glslgram.conditionopt;

import bison;

auto conditionopt() {
  declare_sym(symbol_get("conditionopt"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("conditionopt"));
  grammar_current_rule_symbol_append(symbol_get("condition"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("conditionopt"));
  grammar_current_rule_end();
}
