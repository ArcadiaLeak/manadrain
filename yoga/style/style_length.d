module yoga.style.style_length;

import std.math;
import yoga;

struct StyleLength {
  static StyleLength points(float value) pure {
    return value.isNaN || value.isInfinity
      ? undefined()
      : StyleLength(FloatOptional(value), Unit.Point);
  }

  static StyleLength percent(float value) pure {
    return value.isNaN || value.isInfinity
      ? undefined()
      : StyleLength(FloatOptional(value), Unit.Percent);
  }

  static StyleLength ofAuto() pure {
    return StyleLength(
      FloatOptional(),
      Unit.Auto
    );
  }

  static StyleLength undefined() pure {
    return StyleLength(
      FloatOptional(),
      Unit.Undefined
    );
  }

  private {
    FloatOptional value_;
    Unit unit_;
  }

  bool isAuto() pure {
    return unit_ == Unit.Auto;
  }

  bool isUndefined() pure {
    return unit_ == Unit.Undefined;
  }

  bool isPoints() pure {
    return unit_ == Unit.Point;
  }

  bool isPercent() pure {
    return unit_ == Unit.Percent;
  }

  bool isDefined() pure {
    return !isUndefined();
  }

  FloatOptional value() pure {
    return value_;
  }

  FloatOptional resolve(float referenceLength) pure inout {
    switch (unit_) {
      case Unit.Point:
        return value_;
      case Unit.Percent:
        return FloatOptional(
          value_ * referenceLength * 0.01f
        );
      default:
        return FloatOptional();
    }
  }
}

bool inexactEquals(StyleLength a, StyleLength b) {
  return a.unit_ == b.unit_ &&
    yoga.numeric.inexactEquals(a.value_, b.value_);
}
