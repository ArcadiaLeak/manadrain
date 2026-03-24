module freetype.ftrender;

import freetype;

struct FT_Renderer_Class {
  FT_Module_Class root;

  FT_Glyph_Format glyph_format;
  
  FT_Renderer_RenderFunc render_glyph;
  FT_Renderer_TransformFunc transform_glyph;
  FT_Renderer_GetCBoxFunc get_glyph_cbox;
  FT_Renderer_SetModeFunc set_mode;

  FT_Raster_Funcs* raster_class;
}

struct FT_Glyph_Class {
  FT_Long glyph_size;
  FT_Glyph_Format glyph_format;

  FT_Glyph_InitFunc glyph_init;
  FT_Glyph_DoneFunc glyph_done;
  FT_Glyph_CopyFunc glyph_copy;
  FT_Glyph_TransformFunc glyph_transform;
  FT_Glyph_GetBBoxFunc glyph_bbox;
  FT_Glyph_PrepareFunc glyph_prepare;
}

alias FT_Renderer_RenderFunc = FT_Error function(
  FT_Renderer renderer,
  FT_GlyphSlot slot,
  FT_Render_Mode mode,
  FT_Vector* origin
);

alias FT_Renderer_TransformFunc = FT_Error function(
  FT_Renderer renderer,
  FT_GlyphSlot slot,
  FT_Matrix* matrix,
  FT_Vector* delta
);

alias FT_Renderer_GetCBoxFunc = void function(
  FT_Renderer renderer,
  FT_GlyphSlot slot,
  FT_BBox* cbox
);

alias FT_Renderer_SetModeFunc = FT_Error function(
  FT_Renderer renderer,
  FT_ULong mode_tag,
  FT_Pointer mode_ptr
);

alias FT_Glyph_PrepareFunc = FT_Error function(
  FT_Glyph glyph,
  FT_GlyphSlot slot
);

alias FT_Glyph_GetBBoxFunc = void function(
  FT_Glyph glyph,
  FT_BBox* abbox
);

alias FT_Glyph_TransformFunc = void function(
  FT_Glyph glyph,
  FT_Matrix* matrix,
  FT_Vector* delta
);

alias FT_Glyph_CopyFunc = FT_Error function(
  FT_Glyph source,
  FT_Glyph target
);

alias FT_Glyph_DoneFunc = void function(
  FT_Glyph glyph
);

alias FT_Glyph_InitFunc = FT_Error function(
  FT_Glyph glyph,
  FT_GlyphSlot slot
);
