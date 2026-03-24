module freetype.internal.ftgloadr;

import freetype;

struct FT_SubGlyphRec {
  FT_Int index;
  FT_UShort flags;
  FT_Int arg1;
  FT_Int arg2;
  FT_Matrix transform;
}

struct FT_GlyphLoaderRec {
  FT_Memory memory;
  FT_UInt max_points;
  FT_UInt max_contours;
  FT_UInt max_subglyphs;
  FT_Bool use_extra;

  FT_GlyphLoadRec base;
  FT_GlyphLoadRec current;

  void* other;
}

struct FT_GlyphLoadRec {
  FT_Outline outline;
  FT_Vector* extra_points;
  FT_Vector* extra_points2;
  FT_UInt num_subglyphs;
  FT_SubGlyph subglyphs;
}

alias FT_GlyphLoad = FT_GlyphLoadRec*;

alias FT_GlyphLoader = FT_GlyphLoaderRec*;
