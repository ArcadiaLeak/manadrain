module harfbuzz.hb_face;

import harfbuzz;

struct hb_face_t {
  uint index;
  hb_atomic_t!uint upem;
  hb_atomic_t!uint num_glyphs;

  hb_reference_table_func_t reference_table_func;
  void* user_data;
  hb_destroy_func_t destroy_func;

  // hb_get_table_tags_func_t get_table_tags_func;
  void* get_table_tags_user_data;
  hb_destroy_func_t get_table_tags_destroy_func;

  // hb_shaper_object_dataset!hb_face_t data;
  // hb_ot_face_t table;

  struct plan_node_t {
    hb_shape_plan_t* shape_plan;
    plan_node_t* next;
  }

  hb_atomic_t!(plan_node_t*) shape_plans;
}

alias hb_reference_table_func_t = hb_blob_t* function(
  hb_face_t* face,
  // hb_tag_t tag,
  void* user_data
);
