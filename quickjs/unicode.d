module quickjs.unicode;
import quickjs;

import std.algorithm.iteration;
import std.bitmanip;
import std.range;

immutable ubyte[] unicode_prop_ID_Start_table =
  import("unicode_prop_ID_Start_table.raw");
immutable ubyte[] unicode_prop_ID_Continue1_table =
  import("unicode_prop_ID_Continue1_table.raw");

immutable uint[] unicode_prop_ID_Start_index =
  import("unicode_prop_ID_Start_index.raw").le24_arr;
immutable uint[] unicode_prop_ID_Continue1_index =
  import("unicode_prop_ID_Continue1_index.raw").le24_arr;

ubyte[4] chunk_to_ubyte4(C)(C c) =>
  [c[0], c[1], c[2], 0];

uint ubyte4_to_uint(ubyte[4] c) => 
  c.littleEndianToNative!uint;
  
uint[] le24_arr(const ubyte[] ubyte_arr) =>
  ubyte_arr
    .chunks(3)
    .map!chunk_to_ubyte4
    .map!ubyte4_to_uint
    .array;

enum UNICODE_INDEX_BLOCK_LEN = 32;

long get_index_pos(
  out uint pcode, uint c, const uint[] index_table
) {
  uint code, v;
  size_t idx_min, idx_max, idx;

  idx_min = 0;
  v = index_table[idx_min];
  code = v & ((1 << 21) - 1);
  if (c < code) {
    pcode = 0;
    return 0;
  }
  idx_max = index_table.length - 1;
  code = index_table[idx_max];
  if (c >= code)
    return -1;
  while ((idx_max - idx_min) > 1) {
    idx = (idx_max + idx_min) / 2;
    v = index_table[idx];
    code = v & ((1 << 21) - 1);
    if (c < code) {
      idx_max = idx;
    } else {
      idx_min = idx;
    }
  }
  v = index_table[idx_min];
  pcode = v & ((1 << 21) - 1);

  return (idx_min + 1) * UNICODE_INDEX_BLOCK_LEN + (v >> 21);
}
