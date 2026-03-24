module yoga.style.style_size_length;

import std.math;
import yoga;

struct StyleSizeLength {
  static StyleSizeLength points(float value) pure {
    return value.isNaN || value.isInfinity
      ? undefined()
      : StyleSizeLength(FloatOptional(value), Unit.Point);
  }

  static StyleSizeLength percent(float value) pure {
    return value.isNaN || value.isInfinity
      ? undefined()
      : StyleSizeLength(FloatOptional(value), Unit.Percent);
  }

  static StyleSizeLength ofAuto() pure {
    return StyleSizeLength(
      FloatOptional(),
      Unit.Auto
    );
  }

  static StyleSizeLength ofMaxContent() pure {
    return StyleSizeLength(
      FloatOptional(),
      Unit.MaxContent
    );
  }

  static StyleSizeLength ofFitContent() pure {
    return StyleSizeLength(
      FloatOptional(),
      Unit.FitContent
    );
  }

  static StyleSizeLength ofStretch() pure {
    return StyleSizeLength(
      FloatOptional(),
      Unit.Stretch
    );
  }

  static StyleSizeLength undefined() pure {
    return StyleSizeLength(
      FloatOptional(),
      Unit.Undefined
    );
  }

  private {
    FloatOptional value_;
    Unit unit_;
  }

  FloatOptional value() pure {
    return value_;
  }

  bool isAuto() pure {
    return unit_ == Unit.Auto;
  }

  bool isMaxContent() pure {
    return unit_ == Unit.MaxContent;
  }

  bool isFitContent() pure {
    return unit_ == Unit.FitContent;
  }

  bool isStretch() pure {
    return unit_ == Unit.Stretch;
  }

  bool isUndefined() pure {
    return unit_ == Unit.Undefined;
  }

  bool isDefined() pure {
    return !isUndefined();
  }

  bool isPoints() pure {
    return unit_ == Unit.Point;
  }

  bool isPercent() pure {
    return unit_ == Unit.Percent;
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

bool inexactEquals(StyleSizeLength a, StyleSizeLength b) {
  return a.unit_ == b.unit_ &&
    yoga.numeric.inexactEquals(a.value_, b.value_);
}
