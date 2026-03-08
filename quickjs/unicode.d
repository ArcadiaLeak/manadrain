module quickjs.unicode;
import quickjs;

import std.algorithm.iteration;
import std.bitmanip;
import std.range;

immutable ubyte[] unicode_prop_ID_Start_table =
  import("unicode_prop_ID_Start_table.raw");
immutable ubyte[] unicode_prop_ID_Continue1_table =
  import("unicode_prop_ID_Continue1_table.raw");

immutable ubyte[] lre_ctype_bits =
  import("lre_ctype_bits.raw");

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

enum UNICODE_C_SPACE = 1 << 0;
enum UNICODE_C_DIGIT = 1 << 1;
enum UNICODE_C_UPPER = 1 << 2;
enum UNICODE_C_LOWER = 1 << 3;
enum UNICODE_C_UNDER = 1 << 4;
enum UNICODE_C_DOLLAR = 1 << 5;
enum UNICODE_C_XDIGIT = 1 << 6;
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

int lre_is_in_table(uint c, const(ubyte)[] table, const uint[] index_table) {
  uint code, b, bit;
  long pos;
  const(ubyte)[] p;

  pos = get_index_pos(code, c, index_table);
  if (pos < 0)
    return false;
  p = table[pos..$];
  bit = 0;

  while (true) {
    b = p[0];
    p = p[1..$];
    if (b < 64) {
      code += (b >> 3) + 1;
      if (c < code)
        return bit;
      bit ^= 1;
      code += (b & 7) + 1;
    } else if (b >= 0x80) {
      code += b - 0x80 + 1;
    } else if (b < 0x60) {
      code += (((b - 0x40) << 8) | p[0]) + 1;
      p = p[1..$];
    } else {
      code += (((b - 0x60) << 16) | (p[0] << 8) | p[1]) + 1;
      p = p[2..$];
    }
    if (c < code)
      return bit;
    bit ^= 1;
  }
}

int lre_is_id_start(uint c) =>
  c.lre_is_in_table(
    unicode_prop_ID_Start_table,
    unicode_prop_ID_Start_index
  );

int lre_is_id_continue(uint c) =>
  c.lre_is_id_start ||
  c.lre_is_in_table(
    unicode_prop_ID_Continue1_table,
    unicode_prop_ID_Continue1_index
  );

int lre_is_id_continue_byte(uint c) =>
  lre_ctype_bits[c] & (
    UNICODE_C_UPPER | UNICODE_C_LOWER | UNICODE_C_UNDER |
    UNICODE_C_DOLLAR | UNICODE_C_DIGIT
  );

int lre_js_is_ident_next(uint c) {
  if (c < 128) {
    return lre_is_id_continue_byte(c);
  } else {
    if (c >= 0x200C && c <= 0x200D)
      return true;
    return lre_is_id_continue(c);
  }
}
