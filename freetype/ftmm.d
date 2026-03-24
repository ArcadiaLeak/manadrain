module freetype.ftmm;

import freetype;

struct FT_MM_Var {
  FT_UInt num_axis;
  FT_UInt num_designs;
  FT_UInt num_namedstyles;
  FT_Var_Axis* axis;
  FT_Var_Named_Style* namedstyle;
}

struct FT_Var_Named_Style {
  FT_Fixed* coords;
  FT_UInt strid;
  FT_UInt psid;
}

struct FT_Var_Axis {
  FT_String* name;

  FT_Fixed minimum;
  FT_Fixed def;
  FT_Fixed maximum;

  FT_ULong tag;
  FT_UInt strid;
}
