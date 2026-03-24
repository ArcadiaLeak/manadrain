module freetype.internal.ftserv;

import freetype;

struct FT_ServiceCacheRec {
  FT_Pointer service_POSTSCRIPT_FONT_NAME;
  FT_Pointer service_MULTI_MASTERS;
  FT_Pointer service_METRICS_VARIATIONS;
  FT_Pointer service_GLYPH_DICT;
  FT_Pointer service_PFR_METRICS;
  FT_Pointer service_WINFNT;
}
