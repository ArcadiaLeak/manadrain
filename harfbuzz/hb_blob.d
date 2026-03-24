module harfbuzz.hb_blob;

import harfbuzz;

import core.builtins;

enum hb_memory_mode_t {
  DUPLICATE,
  READONLY,
  WRITABLE,
  READONLY_MAY_MAKE_WRITABLE
}

struct hb_blob_t {
  ~this() { destroy_user_data(); }

  void destroy_user_data() {
    if (destroy_cb) {
      destroy_cb(user_data);
      user_data = null;
      destroy_cb = null;
    }
  }

  private {
    bool try_make_writable();
    bool try_make_writable_inplace();
    bool try_make_writable_inplace_unix();
  }

  char[] as_bytes() inout => data[0..length].dup;
  Type* as(Type)() inout => as_bytes().as!Type(); 

  char* data = null;
  uint length = 0;
  hb_memory_mode_t mode = cast(hb_memory_mode_t) 0;

  void* user_data = null;
  hb_destroy_func_t destroy_cb = null;
}

hb_blob_t hb_blob_create(
  char* data,
  uint length,
  hb_memory_mode_t mode,
  void* user_data,
  hb_destroy_func_t destroy_cb
) {
  if (!length) {
    if (destroy_cb) {
      destroy_cb(user_data);
    }
    return hb_blob_t.init;
  }

  return hb_blob_create_or_fail(
    data, length, mode, user_data, destroy_cb
  );
}

hb_blob_t hb_blob_create_or_fail(
  char* data,
  uint length,
  hb_memory_mode_t mode,
  void* user_data,
  hb_destroy_func_t destroy_cb
) {
  hb_blob_t blob;

  blob.data = data;
  blob.length = length;
  blob.mode = mode;

  blob.user_data = user_data;
  blob.destroy_cb = destroy_cb;

  if (blob.mode == hb_memory_mode_t.DUPLICATE) {
    blob.mode = hb_memory_mode_t.READONLY;
    if (!blob.try_make_writable()) {
      return hb_blob_t.init;
    }
  }

  return blob;
}
