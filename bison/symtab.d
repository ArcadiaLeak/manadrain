module bison.symtab;
import bison;

enum symbol_class_ {
  unknown_sym,
  percent_type_sym,
  token_sym,
  nterm_sym
}

enum int CODE_UNDEFINED = -1;
enum int NUMBER_UNDEFINED = -1;

class symbol {
  string tag;
  int order_of_appearance;

  symbol alias_;
  bool is_alias;

  sym_content content;

  this(string tag) {
    this.tag = tag;
    this.content = new sym_content(this);
  }

  void make_alias(symbol str) {
    str.content = this.content;
    str.content.symbol_ = str;
    str.is_alias = true;
    str.alias_ = this;
    this.alias_ = str;
  }
}

class sym_content {
  symbol symbol_;
  symbol_class_ class_;
  int number;
  int code;

  this(symbol s) {
    symbol_ = s;

    number = NUMBER_UNDEFINED;
    code = CODE_UNDEFINED;

    class_ = symbol_class_.unknown_sym;
  }
}

symbol symbol_get(symbol[string] symbol_table, string key) {
  return symbol_table.require(key, new symbol(key));
}
