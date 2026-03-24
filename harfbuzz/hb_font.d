module harfbuzz.hb_font;

import harfbuzz;

struct hb_font_t {
  hb_atomic_t!uint serial;
  hb_atomic_t!uint serial_coords;

  hb_font_t* parent;
  hb_font_t* face;
  
  int x_scale;
  int y_scale;

  bool is_synthetic;

  float x_embolden;
  float y_embolden;
  bool embolden_in_place;
  int x_strength;
  int y_strength;

  float slant;
  float slant_xy;

  float x_multf;
  float y_multf;
  long x_mult;
  long y_mult;

  uint x_ppem;
  uint y_ppem;

  float ptem;

  uint instance_index;
  bool has_nonzero_coords;
  uint num_coords;
  int* coords;
  float* design_coords;

  // hb_font_funcs_t* klass;
  void* user_data;
  hb_destroy_func_t destroy_func;

  // hb_shaper_object_dataset!hb_font_t data;
}
