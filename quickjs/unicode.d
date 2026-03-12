module quickjs.unicode;

import std.uni;

int lre_js_is_ident_first(uint c) =>
  c.isAlpha || c == '$' || c == '_';

int lre_js_is_ident_next(uint c) =>
  c.isAlphaNum || c == '$' || c == '_';
