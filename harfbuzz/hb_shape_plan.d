module harfbuzz.hb_shape_plan;

import harfbuzz;

struct hb_shape_plan_t {
  // ~this() { key.fini(); }
  hb_face_t* face;
  // hb_shape_plan_key_t key;
  hb_ot_shape_plan_t ot;
}

struct hb_shape_plan_key_t {
  hb_segment_properties_t props;

  hb_feature_t* user_features;
  uint num_user_features;

  hb_shape_func_t shaper_func;
  char* shaper_name;


}