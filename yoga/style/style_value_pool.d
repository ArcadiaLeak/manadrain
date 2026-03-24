module yoga.style.style_value_pool;

import std.conv;
import yoga;

struct StyleValuePool {
  void store(ref StyleValueHandle handle, StyleLength length) {
    if (length.isUndefined()) {
      handle.setType(StyleValueHandle.Type.Undefined);
    } else if (length.isAuto()) {
      handle.setType(StyleValueHandle.Type.Auto);
    } else {
      auto type = length.isPoints()
        ? StyleValueHandle.Type.Point
        : StyleValueHandle.Type.Percent;
      storeValue(handle, length.value(), type);
    }
  }

  void store(ref StyleValueHandle handle, StyleSizeLength sizeValue) {
    if (sizeValue.isUndefined()) {
      handle.setType(StyleValueHandle.Type.Undefined);
    } else if (sizeValue.isAuto()) {
      handle.setType(StyleValueHandle.Type.Auto);
    } else if (sizeValue.isMaxContent()) {
      storeKeyword(handle, StyleValueHandle.Keyword.MaxContent);
    } else if (sizeValue.isStretch()) {
      storeKeyword(handle, StyleValueHandle.Keyword.Stretch);
    } else if (sizeValue.isFitContent()) {
      storeKeyword(handle, StyleValueHandle.Keyword.FitContent);
    } else {
      auto type = sizeValue.isPoints()
        ? StyleValueHandle.Type.Point
        : StyleValueHandle.Type.Percent;
      storeValue(handle, sizeValue.value(), type);
    }
  }

  void store(ref StyleValueHandle handle, FloatOptional number) {
    if (number.isNull) {
      handle.setType(StyleValueHandle.Type.Undefined);
    } else {
      storeValue(handle, number, StyleValueHandle.Type.Number);
    }
  }

  StyleLength getLength(StyleValueHandle handle) pure inout {
    if (handle.isUndefined()) {
      return StyleLength.undefined();
    } else if (handle.isAuto()) {
      return StyleLength.ofAuto();
    } else {
      assert(
        handle.type() == StyleValueHandle.Type.Point ||
        handle.type() == StyleValueHandle.Type.Percent
      );
      uint ivalue = buffer_.get32(handle.value());
      float value = (handle.isValueIndexed())
        ? bitCast!float(ivalue)
        : unpackInlineInteger(handle.value());

      return handle.type() == StyleValueHandle.Type.Point
        ? StyleLength.points(value)
        : StyleLength.percent(value);
    }
  }

  StyleSizeLength getSize(StyleValueHandle handle) pure inout {
    if (handle.isUndefined()) {
      return StyleSizeLength.undefined();
    } else if (handle.isAuto()) {
      return StyleSizeLength.ofAuto();
    } else if (handle.isKeyword(StyleValueHandle.Keyword.MaxContent)) {
      return StyleSizeLength.ofMaxContent();
    } else if (handle.isKeyword(StyleValueHandle.Keyword.FitContent)) {
      return StyleSizeLength.ofFitContent();
    } else if (handle.isKeyword(StyleValueHandle.Keyword.Stretch)) {
      return StyleSizeLength.ofStretch();
    } else {
      assert(
        handle.type() == StyleValueHandle.Type.Point ||
        handle.type() == StyleValueHandle.Type.Percent
      );
      uint ivalue = buffer_.get32(handle.value());
      float value = (handle.isValueIndexed())
        ? bitCast!float(ivalue)
        : unpackInlineInteger(handle.value());

      return handle.type() == StyleValueHandle.Type.Point
        ? StyleSizeLength.points(value)
        : StyleSizeLength.percent(value);
    }
  }

  FloatOptional getNumber(StyleValueHandle handle) pure inout {
    if (handle.isUndefined()) {
      return FloatOptional();
    } else {
      assert(handle.type() == StyleValueHandle.Type.Number);
      uint ivalue = buffer_.get32(handle.value());
      float value = (handle.isValueIndexed())
        ? bitCast!float(ivalue)
        : unpackInlineInteger(handle.value());
      return FloatOptional(value);
    }
  }

  this(ref inout(StyleValuePool) src) pure {
    opAssign(src);
  }

  ref StyleValuePool opAssign(ref inout(StyleValuePool) src) pure {
    foreach (i, ref inout field; src.tupleof)
      this.tupleof[i] = field;
    return this;
  }

private:
  void storeValue(
    ref StyleValueHandle handle,
    float value,
    StyleValueHandle.Type type
  ) {
    handle.setType(type);

    if (handle.isValueIndexed()) {
      auto newIndex = buffer_.replace(
        handle.value(),
        bitCast!uint(value)
      );
      handle.setValue(newIndex);
    } else if (isIntegerPackable(value)) {
      handle.setValue(packInlineInteger(value));
    } else {
      auto newIndex = buffer_.push(bitCast!uint(value));
      handle.setValue(newIndex);
      handle.setValueIsIndexed();
    }
  }

  void storeKeyword(
    ref StyleValueHandle handle,
    StyleValueHandle.Keyword keyword
  ) {
    handle.setType(StyleValueHandle.Type.Keyword);

    if (handle.isValueIndexed()) {
      auto newIndex = buffer_.replace(
        handle.value(),
        cast(uint) keyword
      );
      handle.setValue(newIndex);
    } else {
      handle.setValue(cast(ushort) keyword);
    }
  }

  static const(bool) isIntegerPackable(float f) {
    const ushort kMaxInlineAbsValue = (1 << 11) - 1;

    auto i = cast(int) f;
    return cast(float) i == f && i >= -kMaxInlineAbsValue &&
        i <= +kMaxInlineAbsValue;
  }

  static const(ushort) packInlineInteger(float value) {
    ushort isNegative = value < 0 ? 1 : 0;
    return cast(ushort) (
      (isNegative << 11) |
      (cast(int) value * (isNegative != 0u ? -1 : 1))
    );
  }

  static float unpackInlineInteger(ushort value) pure {
    enum ushort kValueSignMask = 0b0000_1000_0000_0000;
    enum ushort kValueMagnitudeMask = 0b0000_0111_1111_1111;
    const bool isNegative = (value & kValueSignMask) != 0;
    return cast(float) (
      (value & kValueMagnitudeMask) * (isNegative ? -1 : 1)
    );
  }

  SmallValueBuffer buffer_;
}
