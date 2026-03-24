module freetype.ftglyph;

import freetype;

struct FT_GlyphRec {
  FT_Library library;
  FT_Glyph_Class* clazz;
  FT_Glyph_Format format;
  FT_Vector advance;
}

alias FT_Glyph = FT_GlyphRec*;
