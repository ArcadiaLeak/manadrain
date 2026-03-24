module harfbuzz.hb_shaper;

import harfbuzz;

alias hb_shape_func_t = hb_bool_t function(
  hb_shape_plan_t* shape_plan,
  hb_font_t* font,
  hb_feature_t* features,
  uint num_features
);
