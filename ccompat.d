module ccompat;

import core.bitop : bsr;    
import core.checkedint : muls, adds;    

extern(C) {
  bool __builtin_mul_overflow(int a, int b, int* res) {
    bool overflow;
    *res = muls(a, b, overflow);
    return overflow;
  }

  bool __builtin_add_overflow(int a, int b, int* res) {
    bool overflow;
    *res = adds(a, b, overflow);
    return overflow;
  }

  int __builtin_clz(uint v) pure {    
    return 31 - bsr(v);
  }
}
