module freetype.ftincrem;

import freetype;

struct FT_Incremental_InterfaceRec {
  FT_Incremental_FuncsRec* funcs;
  FT_Incremental object;
}

struct FT_Incremental_FuncsRec {
  FT_Incremental_GetGlyphDataFunc get_glyph_data;
  FT_Incremental_FreeGlyphDataFunc free_glyph_data;
  FT_Incremental_GetGlyphMetricsFunc get_glyph_metrics;
}

struct FT_Incremental_MetricsRec {
  FT_Long bearing_x;
  FT_Long bearing_y;
  FT_Long advance;
  FT_Long advance_v;
}

alias FT_Incremental = void*;

alias FT_Incremental_GetGlyphMetricsFunc = FT_Error function(
  FT_Incremental incremental,
  FT_UInt glyph_index,
  FT_Bool vertical,
  FT_Incremental_MetricsRec *ametrics
);

alias FT_Incremental_FreeGlyphDataFunc = void function(
  FT_Incremental incremental,
  FT_Data* data
);

alias FT_Incremental_GetGlyphDataFunc = FT_Error function(
  FT_Incremental incremental,
  FT_UInt glyph_index,
  FT_Data* adata
);
