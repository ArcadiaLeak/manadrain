module harfbuzz.hb_ot_shape;

import harfbuzz;

struct hb_ot_shape_plan_t {
  // ~this() { fini(); }

  // hb_segment_properties_t props;
  // hb_ot_shaper_t* shaper;
  hb_ot_map_t map;
  void* data;
  
  static const hb_mask_t frac_mask = 0;
  static const hb_mask_t numr_mask = 0;
  static const hb_mask_t dnom_mask = 0;

  hb_mask_t rtlm_mask;

  static const hb_mask_t kern_mask = 0;
  static const bool requested_kerning = false;
  static const bool has_frac = false;

  bool has_vert;
  bool has_gpos_mark;
  bool zero_marks;
  bool fallback_glyph_classes;
  bool fallback_mark_positioning;
  bool adjust_mark_positioning_when_zeroing;
  bool apply_gpos;

  static const bool apply_kern = false;

  bool apply_fallback_kern;

  static const bool apply_kerx = false;
  static const bool apply_morx = false;
  static const bool apply_trak = false;


}
