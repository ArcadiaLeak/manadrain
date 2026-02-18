module glslgram.unary_operator;

import bison;

auto unary_operator() {
  declare_sym(symbol_get("unary_operator"), symbol_class_.nterm_sym);

  grammar_current_rule_begin(symbol_get("unary_operator"));
  grammar_current_rule_symbol_append(symbol_get("PLUS"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("unary_operator"));
  grammar_current_rule_symbol_append(symbol_get("DASH"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("unary_operator"));
  grammar_current_rule_symbol_append(symbol_get("BANG"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("unary_operator"));
  grammar_current_rule_symbol_append(symbol_get("TILDE"));
  grammar_current_rule_end();
}
