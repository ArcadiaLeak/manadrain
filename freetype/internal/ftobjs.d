module freetype.internal.ftobjs;

import freetype;

struct FT_LibraryRec {
    FT_Memory memory;

    FT_Int version_major;
    FT_Int version_minor;
    FT_Int version_patch;

    FT_UInt num_modules;
    FT_Module[FT_MAX_MODULES] modules;

    FT_ListRec renderers;
    FT_Renderer cur_renderer;
    FT_Module auto_hinter;

    FT_DebugHook_Func[4] debug_hooks;

  static if (FT_CONFIG_OPTION_SUBPIXEL_RENDERING) {
    FT_LcdFiveTapFilter lcd_weights;
    FT_Bitmap_LcdFilterFunc lcd_filter_func;
  } else {
    FT_Vector[3] lcd_geometry;
  }

    FT_Int refcount;
}

struct FT_ModuleRec {
  FT_Module_Class* clazz;
  FT_Library library;
  FT_Memory memory;
}

struct FT_Slot_InternalRec {
  FT_GlyphLoader loader;
  FT_UInt flags;
  FT_Bool glyph_transformed;
  FT_Matrix glyph_matrix;
  FT_Vector glyph_delta;
  void* glyph_hints;

  FT_Int32 load_flags;
}

struct FT_RendererRec {
  FT_ModuleRec root;
  FT_Renderer_Class* clazz;
  FT_Glyph_Format glyph_format;
  FT_Glyph_Class glyph_class;

  FT_Raster raster;
  FT_Raster_Render_Func raster_render;
  FT_Renderer_RenderFunc render;
}

struct FT_Face_InternalRec {
    FT_Matrix transform_matrix;
    FT_Vector transform_delta;
    FT_Int transform_flags;

    FT_ServiceCacheRec services;

  static if (FT_CONFIG_OPTION_INCREMENTAL)
    FT_Incremental_InterfaceRec* incremental_interface;

    FT_Char no_stem_darkening;
    FT_Int32 random_seed;

  static if (FT_CONFIG_OPTION_SUBPIXEL_RENDERING) {
    FT_LcdFiveTapFilter lcd_weights;
    FT_Bitmap_LcdFilterFunc lcd_filter_func;
  }

    FT_Int refcount;
}

struct FT_Size_InternalRec {
  void* module_data;

  FT_Render_Mode autohint_mode;
  FT_Size_Metrics autohint_metrics;
}

struct FT_DriverRec {
  FT_ModuleRec root;
  FT_Driver_Class clazz;
  FT_ListRec faces_list;
  FT_GlyphLoader glyph_loader;
}
