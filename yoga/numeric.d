module yoga.numeric;

import std.algorithm.comparison;
import std.math;
import std.typecons;

alias FloatOptional = Nullable!(float, float.nan);

unittest {
  FloatOptional a;
  FloatOptional b = FloatOptional(float.nan);

  assert(a.isNull);
  assert(b.isNull);
  assert((b.isNull ? 0.0f : b) == 0.0f);
}

bool inexactEquals(float lhs, float rhs) pure {
  if (!lhs.isNaN && !rhs.isNaN) {
    return abs(lhs - rhs) < 0.0001f;
  }

  return lhs.isNaN && rhs.isNaN;
}

bool inexactEquals(double lhs, double rhs) pure {
  if (!lhs.isNaN && !rhs.isNaN) {
    return abs(lhs - rhs) < 0.0001;
  }

  return lhs.isNaN && rhs.isNaN;
}

bool inexactEquals(float[] lhs, float[] rhs) pure {
  return equal!inexactEquals(lhs, rhs);
}

float maxOrDefined(
  float a,
  float b
) pure {
  if (!a.isNaN && !b.isNaN) {
    return max(a, b);
  }
  return a.isNaN ? b : a;
}

FloatOptional maxOrDefined(
  FloatOptional a,
  FloatOptional b
) pure {
  return FloatOptional(maxOrDefined(a.get, b.get));
}

float minOrDefined(
  float a,
  float b
) pure {
  if (!a.isNaN && !b.isNaN) {
    return min(a, b);
  }

  return a.isNaN ? b : a;
}
