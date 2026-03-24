module freetype.internal.ftmemory;

import freetype;

import std.conv;

string FT_FREE(string ptr) => FT_MEM_FREE(ptr);
string FT_ASSIGNP(string p, string val) => iq{ ($(p)) = ($(val)) }.text;

string FT_DEBUG_INNER(string exp) => iq{ ($(exp)) }.text;
string FT_ASSIGNP_INNER(string p, string exp) => FT_ASSIGNP(p, exp);

string FT_MEM_FREE(string ptr) {
  return iq{
    $(FT_BEGIN_STMNT)
      $(FT_DEBUG_INNER(
        iq{ft_mem_free( memory, ($(ptr)) )}.text
      ));
      ($(ptr)) = null;
    $(FT_END_STMNT)
  }.text;
}

string FT_MEM_ALLOC(string ptr, string size) {
  return FT_ASSIGNP_INNER(
    ptr,
    iq{ft_mem_alloc(
      memory,
      cast(FT_Long) ($(size)),
      &error
    )}.text
  );
}
