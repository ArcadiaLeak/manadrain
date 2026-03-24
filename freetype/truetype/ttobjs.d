module freetype.truetype.ttobjs;

import freetype;

struct TT_Size_Metrics {
  FT_Long x_ratio;
  FT_Long y_ratio;

  FT_Long ratio;
  FT_Fixed scale;
  FT_UShort ppem;

  FT_Bool rotated;
  FT_Bool stretched;
}

struct TT_SizeRec {
    FT_SizeRec root;

    FT_Size_Metrics* metrics;
    FT_Size_Metrics hinted_metrics;

    TT_Size_Metrics ttmetrics;

    FT_Byte* widthp;

    FT_ULong strike_index;

  static if (TT_USE_BYTECODE_INTERPRETER) {
    FT_Long point_size;

    TT_GraphicsState GS;

    TT_GlyphZoneRec twilight;

    TT_ExecContext context;

    FT_Error bytecode_ready;
    FT_Error cvt_ready;
  }
}

struct TT_DriverRec {
  FT_DriverRec root;

  TT_GlyphZoneRec zone;

  FT_UInt interpreter_version;
}

alias TT_Driver = TT_DriverRec*;
