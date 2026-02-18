module bison.symtab;
import bison;

enum symbol_class_ {
  unknown_sym,
  percent_type_sym,
  token_sym,
  nterm_sym
}

struct symbol_number {
  int _;
  alias _ this;
}

enum int CODE_UNDEFINED = -1;
enum int NUMBER_UNDEFINED = -1;

symbol acceptsymbol;
symbol errtoken;
symbol undeftoken;
symbol eoftoken;

symbol[string] symbol_table;
symbol[] symbols_sorted;

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
  symbol_number number;
  int code;

  this(symbol s) {
    symbol_ = s;

    number = symbol_number(NUMBER_UNDEFINED);
    code = CODE_UNDEFINED;

    class_ = symbol_class_.unknown_sym;
  }
}

symbol declare_sym(symbol sym, symbol_class_ class_) {
  static int order_of_appearance = 0;
  sym.content.class_ = class_;
  sym.order_of_appearance = ++order_of_appearance;
  return sym;
}

symbol symbol_get(string key) {
  return symbol_table.require(key, new symbol(key));
}

symbol dummy_symbol_get() {
  import std.format;

  static int dummy_count = 0;
  return declare_sym(
    symbol_get(format("$@%d", ++dummy_count)),
    symbol_class_.nterm_sym
  );
}


void symbols_check_defined() {
  import std.algorithm.sorting;
  import std.array;
  symbols_sorted = symbol_table.values
    .sort!("a.order_of_appearance < b.order_of_appearance")
    .array;

  foreach (sym; symbols_sorted)
    if (sym.content.number == NUMBER_UNDEFINED)
      sym.content.number = sym.content.class_ == symbol_class_.token_sym
        ? ntokens++ : nnterms++;
}

void symbols_pack() {
  symbols = new symbol[nsyms];
  foreach (sym; symbols_sorted)
    sym.symbol_pack;

  symbols_token_translations_init;
}

void symbol_pack(symbol sym) {
  if (sym.content.class_ == symbol_class_.nterm_sym)
    sym.content.number += ntokens;

  symbols[sym.content.number] = sym.content.symbol_;
}

void symbols_token_translations_init() {
  bool code_256_available_p = true;

  max_code = 0;
  foreach (s; symbols[0..ntokens]) {
    sym_content sym = s.content;
    if (sym.code != CODE_UNDEFINED) {
      if (sym.code > max_code)
        max_code = sym.code;
      if (sym.code == 256)
        code_256_available_p = false;
    }
  }

  if (code_256_available_p && errtoken.content.code == CODE_UNDEFINED)
    errtoken.content.code = 256;

  if (max_code < 256)
    max_code = 256;

  foreach (s; symbols[0..ntokens]) {
    sym_content sym = s.content;
    if (sym.code == CODE_UNDEFINED)
      sym.code = ++max_code;
    if (sym.code > max_code)
      max_code = sym.code;
  }

  token_translations = new symbol_number[max_code + 1];
  token_translations[] = undeftoken.content.number;

  foreach (sym; symbols_sorted)
    sym.symbol_translation;
}

void symbol_translation(symbol sym) {
  if (sym.content.class_ == symbol_class_.token_sym && !sym.is_alias)
    token_translations[sym.content.code] = sym.content.number;
}

string symbol_id_get(symbol sym) {
  if (sym.alias_)
    return sym.alias_.tag;
  else
    return sym.tag;
}
