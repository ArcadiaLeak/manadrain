module yoga.node.cached_measurement;

import std.math;
import yoga;

struct CachedMeasurement {
  float availableWidth = -1;
  float availableHeight = -1;
  SizingMode widthSizingMode = SizingMode.MaxContent;
  SizingMode heightSizingMode = SizingMode.MaxContent;

  float computedWidth = -1;
  float computedHeight = -1;

  bool opEquals(CachedMeasurement measurement) pure {
    bool isEqual = widthSizingMode == measurement.widthSizingMode &&
      heightSizingMode == measurement.heightSizingMode;

    if (
      !availableWidth.isNaN ||
      !measurement.availableWidth.isNaN
    ) {
      isEqual = isEqual && availableWidth == measurement.availableWidth;
    }
    if (
      !availableHeight.isNaN ||
      !measurement.availableHeight.isNaN
    ) {
      isEqual = isEqual && availableHeight == measurement.availableHeight;
    }
    if (
      !computedWidth.isNaN ||
      !measurement.computedWidth.isNaN
    ) {
      isEqual = isEqual && computedWidth == measurement.computedWidth;
    }
    if (
      !computedHeight.isNaN ||
      !measurement.computedHeight.isNaN
    ) {
      isEqual = isEqual && computedHeight == measurement.computedHeight;
    }

    return isEqual;
  }
}