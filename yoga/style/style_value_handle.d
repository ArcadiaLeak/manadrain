module yoga.style.style_value_handle;

import yoga;

struct StyleValueHandle {
  static StyleValueHandle ofAuto() {
    StyleValueHandle handle;
    handle.setType(Type.Auto);
    return handle;
  }

  bool isUndefined() pure inout {
    return type() == Type.Undefined;
  }

  bool isDefined() pure inout {
    return !isUndefined();
  }

  bool isAuto() pure inout {
    return type() == Type.Auto;
  }

package:
  static const ushort kHandleTypeMask = 0b0000_0000_0000_0111;
  static const ushort kHandleIndexedMask = 0b0000_0000_0000_1000;
  static const ushort kHandleValueMask = 0b1111_1111_1111_0000;

  enum Type : ubyte {
    Undefined,
    Point,
    Percent,
    Number,
    Auto,
    Keyword
  };

  enum Keyword : ubyte { MaxContent, FitContent, Stretch };

  bool isKeyword(Keyword keyword) pure {
    return type() == Type.Keyword && value() == cast(ushort) keyword;
  }

  Type type() pure inout {
    return cast(Type) (repr_ & kHandleTypeMask);
  }

  void setType(Type handleType) {
    repr_ &= (~kHandleTypeMask);
    repr_ |= cast(ubyte) handleType;
  }

  ushort value() pure {
    return repr_ >> 4;
  }

  void setValue(ushort value) {
    repr_ &= (~kHandleValueMask);
    repr_ |= (value << 4);
  }

  bool isValueIndexed() pure {
    return (repr_ & kHandleIndexedMask) != 0;
  }

  void setValueIsIndexed() {
    repr_ |= kHandleIndexedMask;
  }

  ushort repr_ = 0;
}
