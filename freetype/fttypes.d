module freetype.fttypes;

import freetype;

alias FT_Bool = ubyte;
alias FT_Char = byte;
alias FT_Byte = ubyte;
alias FT_Bytes = FT_Byte*;
alias FT_String = char;
alias FT_Short = short;
alias FT_UShort = ushort;
alias FT_Int = int;
alias FT_UInt = uint;
alias FT_Long = long;
alias FT_ULong = ulong;
alias FT_F2Dot14 = short;
alias FT_F26Dot6 = long;
alias FT_Fixed = long;
alias FT_Offset = size_t;
alias FT_Pointer = void*;
alias FT_List = FT_ListRec*;
alias FT_ListNode = FT_ListNodeRec*;
alias FT_Generic_Finalizer = void function(void* object);

struct FT_ListRec {
  FT_ListNode head;
  FT_ListNode tail;
}

struct FT_ListNodeRec {
  FT_ListNode prev;
  FT_ListNode next;
  void* data;
}

struct FT_Generic {
  void* data;
  FT_Generic_Finalizer finalizer;
}

struct FT_Matrix {
  FT_Fixed xx, xy;
  FT_Fixed yx, yy;
}

struct FT_Data {
  FT_Byte* pointer;
  FT_UInt length;
}

struct FT_UnitVector {
  FT_F2Dot14 x;
  FT_F2Dot14 y;
}
