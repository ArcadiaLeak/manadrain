module harfbuzz.hb_common;

struct hb_feature_t {
  // hb_tag_t tag;
  uint value;
  uint start;
  uint end;
}

alias hb_destroy_func_t = void function(
  void* user_data
);

alias hb_mask_t = uint;
alias hb_bool_t = int;