module freetype.internal.tttypes;

import freetype;

enum TT_SbitTableType {
  TT_SBIT_TABLE_TYPE_NONE = 0,
  TT_SBIT_TABLE_TYPE_EBLC,
  TT_SBIT_TABLE_TYPE_CBLC,
  TT_SBIT_TABLE_TYPE_SBIX,
  TT_SBIT_TABLE_TYPE_MAX
}

struct TT_GlyphZoneRec {
  FT_UShort n_points;
  FT_UShort n_contours;

  FT_Vector* org;
  FT_Vector* cur;
  FT_Vector* orus;

  FT_Byte* tags;
  FT_UShort* contours;

  FT_UShort first_point;
}

struct TT_FaceRec {
    FT_FaceRec root;

    TTC_HeaderRec ttc_header;

    FT_ULong format_tag;
    FT_UShort num_tables;
    TT_Table dir_tables;

    TT_Header header;
    TT_HoriHeader horizontal;

    TT_MaxProfile max_profile;

    FT_Bool vertical_info;
    TT_VertHeader vertical;

    FT_UShort num_names;
    TT_NameTableRec name_table;

    TT_OS2 os2;
    TT_Postscript postscript;

    FT_Byte* cmap_table;
    FT_ULong cmap_size;

    TT_Loader_GotoTableFunc goto_table;

    TT_Loader_StartGlyphFunc access_glyph_frame;
    TT_Loader_EndGlyphFunc forget_glyph_frame;
    TT_Loader_ReadGlyphFunc read_glyph_header;
    TT_Loader_ReadGlyphFunc read_simple_glyph;
    TT_Loader_ReadGlyphFunc read_composite_glyph;

    void* sfnt;

    void* psnames;

  static if (TT_CONFIG_OPTION_GX_VAR_SUPPORT) {
    void* mm;

    void* tt_var;

    void* face_var;
  }

    void* psaux;

    TT_GaspRec gasp;

    TT_PCLT pclt;

    FT_ULong num_sbit_scales;
    TT_SBit_Scale sbit_scales;

    TT_Post_NamesRec postscript_names;

    FT_Palette_Data palette_data;
    FT_UShort palette_index;
    FT_Color* palette;
    FT_Bool have_foreground_color;
    FT_Color foreground_color;

    FT_ULong font_program_size;
    FT_Byte* font_program;

    FT_ULong cvt_program_size;
    FT_Byte* cvt_program;

    FT_ULong cvt_size;
    FT_Int32* cvt;

    FT_Generic extra;

    char* postscript_name;

    FT_ULong glyph_len;
    FT_ULong glyph_offset;

    FT_Bool is_cff2;

  static if (TT_CONFIG_OPTION_GX_VAR_SUPPORT) {
    FT_Bool doblend;
    GX_Blend blend;

    FT_UInt32 variation_support;

    char* var_postscript_prefix;
    FT_UInt var_postscript_prefix_len;

    FT_UInt var_default_named_instance;

    char* non_var_style_name;
  }

    FT_ULong horz_metrics_size;
    FT_ULong vert_metrics_size;

    FT_ULong num_locations;
    FT_Byte* glyph_locations;

    FT_Byte* hdmx_table;
    FT_ULong hdmx_table_size;
    FT_UInt hdmx_record_count;
    FT_ULong hdmx_record_size;
    FT_Byte** hdmx_records;

    FT_Byte* sbit_table;
    FT_ULong sbit_table_size;
    TT_SbitTableType sbit_table_type;
    FT_UInt sbit_num_strikes;
    FT_UInt* sbit_strike_map;

    FT_Byte* kern_table;
    FT_ULong kern_table_size;
    FT_UInt num_kern_tables;
    FT_UInt32 kern_avail_bits;
    FT_UInt32 kern_order_bits;

  static if (TT_CONFIG_OPTION_BDF) {
    TT_BDFRec bdf;
  }

    FT_ULong horz_metrics_offset;
    FT_ULong vert_metrics_offset;

  static if (TT_CONFIG_OPTION_EMBEDDED_BITMAPS) {
    FT_ULong ebdt_start;
    FT_ULong ebdt_size;
  }

    void* cpal;
    void* colr;

    void* svg;

  static if (TT_CONFIG_OPTION_GPOS_KERNING) {
    FT_Byte* gpos_table;

    FT_UInt32* gpos_lookups_kerning;
    FT_UInt num_gpos_lookups_kerning;
  }
}

struct TT_SBit_ScaleRec {
  TT_SBit_LineMetricsRec hori;
  TT_SBit_LineMetricsRec vert;

  FT_Byte x_ppem;
  FT_Byte y_ppem;

  FT_Byte x_ppem_substitute;
  FT_Byte y_ppem_substitute;
}

struct TT_Post_NamesRec {
  FT_Bool loaded;
  FT_UShort num_glyphs;
  FT_UShort num_names;
  FT_UShort* glyph_indices;
  FT_Byte** glyph_names;
}

struct TT_SBit_LineMetricsRec {
  FT_Char ascender;
  FT_Char descender;
  FT_Byte max_width;
  FT_Char caret_slope_numerator;
  FT_Char caret_slope_denominator;
  FT_Char caret_offset;
  FT_Char min_origin_SB;
  FT_Char min_advance_SB;
  FT_Char max_before_BL;
  FT_Char min_after_BL;
  FT_Char[2] pads;
}

struct TT_BDFRec {
  FT_Byte* table;
  FT_Byte* table_end;
  FT_Byte* strings;
  FT_ULong strings_size;
  FT_UInt num_strikes;
  FT_Bool loaded;
}

struct TT_GaspRec {
  FT_UShort version_;
  FT_UShort numRanges;
  TT_GaspRange gaspRanges;
}

struct TT_LoaderRec {
  TT_Face face;
  TT_Size size;
  FT_GlyphSlot glyph;
  FT_GlyphLoader gloader;

  FT_ULong load_flags;
  FT_UInt glyph_index;

  FT_Stream stream;
  FT_UInt byte_len;

  FT_Short n_contours;
  FT_BBox bbox;
  FT_Int left_bearing;
  FT_Int advance;
  FT_Int linear;
  FT_Bool linear_def;
  FT_Vector pp1;
  FT_Vector pp2;

  TT_GlyphZoneRec base;
  TT_GlyphZoneRec zone;

  TT_ExecContext exec;
  FT_ULong ins_pos;

  void* other;

  FT_Int top_bearing;
  FT_Int vadvance;
  FT_Vector pp3;
  FT_Vector pp4;

  FT_Byte* cursor;
  FT_Byte* limit;

  FT_ListRec composites;

  FT_Byte* widthp;
}

struct TT_GaspRangeRec {
  FT_UShort maxPPEM;
  FT_UShort gaspFlag;
}

struct TT_NameTableRec {
  FT_UShort format;
  FT_UInt numNameRecords;
  FT_UInt storageOffset;
  TT_NameRec* names;
  FT_UInt numLangTagRecords;
  TT_LangTagRec* langTags;
  FT_Stream stream;
}

struct TT_LangTagRec {
  FT_UShort stringLength;
  FT_ULong stringOffset;

  FT_Byte* string;
}

struct TT_NameRec {
  FT_UShort platformID;
  FT_UShort encodingID;
  FT_UShort languageID;
  FT_UShort nameID;
  FT_UShort stringLength;
  FT_ULong stringOffset;

  FT_Byte* string;
}

struct TT_TableRec {
  FT_ULong Tag;
  FT_ULong CheckSum;
  FT_ULong Offset;
  FT_ULong Length;
}

struct TTC_HeaderRec {
  FT_ULong tag;
  FT_Fixed version_;
  FT_Long count;
  FT_ULong* offsets;
}

alias TT_Table = TT_TableRec*;
alias TT_Name = TT_NameRec*;
alias TT_LangTag = TT_LangTagRec*;
alias TT_NameTable = TT_NameTableRec*;
alias TT_Loader = TT_LoaderRec*;
alias TT_GaspRange = TT_GaspRangeRec*;
alias TT_SBit_LineMetrics = TT_SBit_LineMetricsRec*;
alias TT_ExecContext = TT_ExecContextRec*;
alias TT_GlyphZone = TT_GlyphZoneRec*;
alias TT_Size = TT_SizeRec*;
alias TT_SBit_Scale = TT_SBit_ScaleRec*;
alias TT_Post_Names = TT_Post_NamesRec*;
alias TT_Face = TT_FaceRec*;
alias TT_BDF = TT_BDFRec*;

alias TT_Interpreter = FT_Error function(
  void* exec_context
);

alias TT_Loader_ReadGlyphFunc = FT_Error function(
  TT_Loader loader
);

alias TT_Loader_EndGlyphFunc = void function(
  TT_Loader loader
);

alias TT_Loader_StartGlyphFunc = FT_Error function(
  TT_Loader loader,
  FT_UInt glyph_index,
  FT_ULong offset,
  FT_UInt byte_count
);

alias TT_Loader_GotoTableFunc = FT_Error function(
  TT_Face face,
  FT_ULong tag,
  FT_Stream stream,
  FT_ULong* length
);
