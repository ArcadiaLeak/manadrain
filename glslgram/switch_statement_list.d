module glslgram.switch_statement_list;

import bison;

auto switch_statement_list() {
	declare_sym(symbol_get("switch_statement_list"), symbol_class_.nterm_sym);

	grammar_current_rule_begin(symbol_get("switch_statement_list"));
	grammar_current_rule_end();

	grammar_current_rule_begin(symbol_get("switch_statement_list"));
	grammar_current_rule_symbol_append(symbol_get("statement_list"));
	grammar_current_rule_end();
}
