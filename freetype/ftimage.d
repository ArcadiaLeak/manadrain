module freetype.ftimage;

import freetype;

import std.conv;

uint FT_IMAGE_TAG(char[4] bytes) =>
  cast(uint) bytes[0] << 24 | 
  cast(uint) bytes[1] << 16 | 
  cast(uint) bytes[2] << 8  | 
  cast(uint) bytes[3];

enum FT_Glyph_Format : uint {
  FT_GLYPH_FORMAT_NONE = [0, 0, 0, 0].FT_IMAGE_TAG,

  FT_GLYPH_FORMAT_COMPOSITE = "comp".FT_IMAGE_TAG,
  FT_GLYPH_FORMAT_BITMAP = "bits".FT_IMAGE_TAG,
  FT_GLYPH_FORMAT_OUTLINE = "outl".FT_IMAGE_TAG,
  FT_GLYPH_FORMAT_PLOTTER = "plot".FT_IMAGE_TAG,
  FT_GLYPH_FORMAT_SVG = "SVG ".FT_IMAGE_TAG
}

alias FT_Pos = long;

alias FT_Raster_NewFunc = int function(
  void* memory,
  FT_Raster* raster
);

alias FT_Raster_DoneFunc = void function(
  FT_Raster raster
);

alias FT_Raster_ResetFunc = void function(
  FT_Raster raster,
  char* pool_base,
  ulong pool_size
);

alias FT_Raster_SetModeFunc = int function(
  FT_Raster raster,
  ulong mode,
  void* args
);

alias FT_Raster_RenderFunc = int function(
  FT_Raster raster,
  FT_Raster_Params* params
);

alias FT_Raster_Render_Func = FT_Raster_RenderFunc;

alias FT_SpanFunc = void function(
  int y,
  int count,
  FT_Span* spans,
  void* user
);

struct FT_Span {
  short x;
  ushort len;
  char coverage;
}

struct FT_Vector {
  FT_Pos x;
  FT_Pos y;
}

struct FT_BBox {
  FT_Pos xMin, yMin;
  FT_Pos xMax, yMax;
}

struct FT_Raster_Funcs {
  FT_Glyph_Format glyph_format;

  FT_Raster_NewFunc raster_new;
  FT_Raster_ResetFunc raster_reset;
  FT_Raster_SetModeFunc raster_set_mode;
  FT_Raster_RenderFunc raster_render;
  FT_Raster_DoneFunc raster_done;
}

struct FT_Outline {
  ushort n_contours;
  ushort n_points;

  FT_Vector* points;
  char* tags;
  ushort* contours;

  int flags;
}

struct FT_Bitmap {
  uint rows;
  uint width;
  int pitch;
  ubyte* buffer;
  ushort num_grays;
  ubyte pixel_mode;
  ubyte palette_mode;
  void* palette;
}

struct FT_Raster_Params {
  FT_Bitmap* target;
  void* source;
  int flags;
  FT_SpanFunc gray_spans;
  size_t black_spans;
  size_t bit_test;
  size_t bit_set;
  void* user;
  FT_BBox clip_box;
}
