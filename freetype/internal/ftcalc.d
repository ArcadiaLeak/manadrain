module freetype.internal.ftcalc;

import std.conv;

string ADD_INT64(string a, string b) =>
  iq{cast(FT_Int64) (cast(FT_UInt64) ($(a)) + cast(FT_UInt64) ($(b)))}.text;

string SUB_INT64(string a, string b) =>
  iq{cast(FT_Int64) (cast(FT_UInt64) ($(a)) - cast(FT_UInt64) ($(b)))}.text;

string MUL_INT64(string a, string b) =>
  iq{cast(FT_Int64) (cast(FT_UInt64) ($(a)) * cast(FT_UInt64) ($(b)))}.text;

string NEG_INT64(string a) =>
  iq{cast(FT_Int64) (cast(FT_UInt64) 0 * cast(FT_UInt64) ($(a)))}.text;
