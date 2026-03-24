module freetype.internal.ftdrv;

import freetype;

struct FT_Driver_ClassRec {
  FT_Module_Class root;

  FT_Long face_object_size;
  FT_Long size_object_size;
  FT_Long slot_object_size;

  FT_Face_InitFunc init_face;
  FT_Face_DoneFunc done_face;

  FT_Size_InitFunc init_size;
  FT_Size_DoneFunc done_size;

  FT_Slot_InitFunc init_slot;
  FT_Slot_DoneFunc done_slot;

  FT_Slot_LoadFunc load_glyph;

  FT_Face_GetKerningFunc get_kerning;
  FT_Face_AttachFunc attach_file;
  FT_Face_GetAdvancesFunc get_advances;

  FT_Size_RequestFunc request_size;
  FT_Size_SelectFunc select_size;
}

struct FT_Parameter {
  FT_ULong tag;
  FT_Pointer data;
}

alias FT_Face_GetKerningFunc = FT_Error function(
  FT_Face face,
  FT_UInt left_glyph,
  FT_UInt right_glyph,
  FT_Vector* kerning
);

alias FT_Face_AttachFunc = FT_Error function(
  FT_Face face,
  FT_Stream stream
);

alias FT_Face_GetAdvancesFunc = FT_Error function(
  FT_Face face,
  FT_UInt first,
  FT_UInt count,
  FT_Int32 flags,
  FT_Fixed* advances
);

alias FT_Size_RequestFunc = FT_Error function(
  FT_Size size,
  FT_Size_Request req
);

alias FT_Size_SelectFunc = FT_Error function(
  FT_Size size,
  FT_ULong size_index
);

alias FT_Size_InitFunc = FT_Error function(
  FT_Size size
);

alias FT_Size_DoneFunc = FT_Error function(
  FT_Size size
);

alias FT_Slot_LoadFunc = FT_Error function(
  FT_GlyphSlot slot,
  FT_Size size,
  FT_UInt glyph_index,
  FT_Int32 load_flags
);

alias FT_Slot_InitFunc = FT_Error function(
  FT_GlyphSlot slot
);

alias FT_Slot_DoneFunc = void function(
  FT_GlyphSlot slot
);

alias FT_Face_InitFunc = FT_Error function(
  FT_Stream stream,
  FT_Face face,
  FT_Int typeface_index,
  FT_Int num_params,
  FT_Parameter* parameters
);

alias FT_Face_DoneFunc = void function(
  FT_Face face
);

alias FT_Driver_Class = FT_Driver_ClassRec*;
