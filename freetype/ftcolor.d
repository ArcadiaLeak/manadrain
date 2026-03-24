module freetype.ftcolor;

import freetype;

struct FT_Palette_Data {
  FT_UShort num_palettes;
  FT_UShort* palette_name_ids;
  FT_UShort* palette_flags;

  FT_UShort num_palette_entries;
  FT_UShort* palette_entry_name_ids;
}

struct FT_Color {
  FT_Byte blue;
  FT_Byte green;
  FT_Byte red;
  FT_Byte alpha;
}
