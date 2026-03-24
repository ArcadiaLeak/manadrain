module freetype;

public import freetype.base;
public import freetype.config;
public import freetype.internal;
public import freetype.truetype;

public import freetype.ftcolor;
public import freetype.fterrdef;
public import freetype.ftglyph;
public import freetype.ftimage;
public import freetype.ftincrem;
public import freetype.ftmm;
public import freetype.ftmodapi;
public import freetype.ftrender;
public import freetype.ftsystem;
public import freetype.fttypes;
public import freetype.tttables;

import std.conv;

uint FT_ENC_TAG(string bytes) =>
  cast(uint) bytes[0] << 24 | 
  cast(uint) bytes[1] << 16 | 
  cast(uint) bytes[2] << 8  | 
  cast(uint) bytes[3];

enum FT_Encoding : FT_UInt32 {
  FT_ENCODING_NONE = [0, 0, 0, 0].FT_ENC_TAG,

  FT_ENCODING_MS_SYMBOL = "symb".FT_ENC_TAG,
  FT_ENCODING_UNICODE = "unic".FT_ENC_TAG,

  FT_ENCODING_SJIS = "sjis".FT_ENC_TAG,
  FT_ENCODING_PRC = "gb  ".FT_ENC_TAG,
  FT_ENCODING_BIG5 = "big5".FT_ENC_TAG,
  FT_ENCODING_WANSUNG = "wans".FT_ENC_TAG,
  FT_ENCODING_JOHAB = "joha".FT_ENC_TAG,

  FT_ENCODING_GB2312 = FT_ENCODING_PRC,
  FT_ENCODING_MS_SJIS = FT_ENCODING_SJIS,
  FT_ENCODING_MS_GB2312 = FT_ENCODING_PRC,
  FT_ENCODING_MS_BIG5 = FT_ENCODING_BIG5,
  FT_ENCODING_MS_WANSUNG = FT_ENCODING_WANSUNG,
  FT_ENCODING_MS_JOHAB = FT_ENCODING_JOHAB,

  FT_ENCODING_ADOBE_STANDARD = "ADOB".FT_ENC_TAG,
  FT_ENCODING_ADOBE_EXPERT = "ADBE".FT_ENC_TAG,
  FT_ENCODING_ADOBE_CUSTOM = "ADBC".FT_ENC_TAG,
  FT_ENCODING_ADOBE_LATIN_1 = "lat1".FT_ENC_TAG,

  FT_ENCODING_OLD_LATIN_2 = "lat2".FT_ENC_TAG,

  FT_ENCODING_APPLE_ROMAN = "armn".FT_ENC_TAG
}

enum FT_Render_Mode {
  FT_RENDER_MODE_NORMAL = 0,
  FT_RENDER_MODE_LIGHT,
  FT_RENDER_MODE_MONO,
  FT_RENDER_MODE_LCD,
  FT_RENDER_MODE_LCD_V,
  FT_RENDER_MODE_SDF,

  FT_RENDER_MODE_MAX
}

enum FT_Size_Request_Type {
  FT_SIZE_REQUEST_TYPE_NOMINAL,
  FT_SIZE_REQUEST_TYPE_REAL_DIM,
  FT_SIZE_REQUEST_TYPE_BBOX,
  FT_SIZE_REQUEST_TYPE_CELL,
  FT_SIZE_REQUEST_TYPE_SCALES,

  FT_SIZE_REQUEST_TYPE_MAX
}

struct FT_Glyph_Metrics {
  FT_Pos width;
  FT_Pos height;

  FT_Pos horiBearingX;
  FT_Pos horiBearingY;
  FT_Pos horiAdvance;
  
  FT_Pos vertBearingX;
  FT_Pos vertBearingY;
  FT_Pos vertAdvance;
}

struct FT_Bitmap_Size {
  FT_Short height;
  FT_Short width;

  FT_Pos size;

  FT_Pos x_ppem;
  FT_Pos y_ppem;
}

struct FT_GlyphSlotRec {
  FT_Library library;
  FT_Face face;
  FT_GlyphSlot next;
  FT_UInt glyph_index;
  FT_Generic generic;

  FT_Glyph_Metrics metrics;
  FT_Fixed linearHoriAdvance;
  FT_Fixed linearVertAdvance;
  FT_Vector advance;

  FT_Glyph_Format format;

  FT_Bitmap bitmap;
  FT_Int bitmap_left;
  FT_Int bitmap_top;

  FT_Outline outline;

  FT_UInt num_subglyphs;
  FT_SubGlyph subglyphs;

  void* control_data;
  long control_len;

  FT_Pos lsb_delta;
  FT_Pos rsb_delta;

  void* other;
  
  FT_Slot_Internal internal;
}

struct FT_FaceRec {
  FT_Long num_faces;
  FT_Long face_index;

  FT_Long face_flags;
  FT_Long style_flags;

  FT_Long num_glyphs;

  FT_String* family_name;
  FT_String* style_name;

  FT_Int num_fixed_sizes;
  FT_Bitmap_Size* available_sizes;

  FT_Int num_charmaps;
  FT_CharMap* charmaps;

  FT_Generic generic;

  FT_BBox bbox;

  FT_UShort units_per_EM;
  FT_Short ascender;
  FT_Short descender;
  FT_Short height;

  FT_Short max_advance_width;
  FT_Short max_advance_height;

  FT_Short underline_position;
  FT_Short underline_thickness;

  FT_GlyphSlot glyph;
  FT_Size size;
  FT_CharMap charmap;

  FT_Driver driver;
  FT_Memory memory;
  FT_Stream stream;

  FT_ListRec sizes_list;

  FT_Generic autohint;
  void* extensions;

  FT_Face_Internal internal;
}

struct FT_CharMapRec {
  FT_Face face;
  FT_Encoding encoding;
  FT_UShort platform_id;
  FT_UShort encoding_id;
}

struct FT_SizeRec {
  FT_Face face;
  FT_Generic generic;
  FT_Size_Metrics metrics;
  FT_Size_Internal internal;
}

struct FT_Size_Metrics {
  FT_UShort x_ppem;
  FT_UShort y_ppem;

  FT_Fixed x_scale;
  FT_Fixed y_scale;

  FT_Pos ascender;
  FT_Pos descender;
  FT_Pos height;
  FT_Pos max_advance;
}

struct FT_Size_RequestRec {
  FT_Size_Request_Type type;
  FT_Long width;
  FT_Long height;
  FT_UInt horiResolution;
  FT_UInt vertResolution;
}

alias FT_Library = FT_LibraryRec*;
alias FT_Module = FT_ModuleRec*;
alias FT_Renderer = FT_RendererRec*;
alias FT_GlyphSlot = FT_GlyphSlotRec*;
alias FT_Slot_Internal = FT_Slot_InternalRec*;
alias FT_SubGlyph = FT_SubGlyphRec*;
alias FT_CharMap = FT_CharMapRec*;
alias FT_Face = FT_FaceRec*;
alias FT_Face_Internal = FT_Face_InternalRec*;
alias FT_Size = FT_SizeRec*;
alias FT_Size_Internal = FT_Size_InternalRec*;
alias FT_Driver = FT_DriverRec*;
alias FT_Size_Request = FT_Size_RequestRec*;
alias FT_Raster = void*;
