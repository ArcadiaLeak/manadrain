module yoga.algorithm.cache;

import std.math;
import yoga;

private:
bool sizeIsExactAndMatchesOldMeasuredSize(
  SizingMode sizeMode,
  float size,
  float lastComputedSize
) pure {
  return sizeMode == SizingMode.StretchFit &&
    size.inexactEquals(lastComputedSize);
}

bool oldSizeIsMaxContentAndStillFits(
  SizingMode sizeMode,
  float size,
  SizingMode lastSizeMode,
  float lastComputedSize
) pure {
  return sizeMode == SizingMode.FitContent &&
    lastSizeMode == SizingMode.MaxContent &&
    (size >= lastComputedSize || size.inexactEquals(lastComputedSize));
}

bool newSizeIsStricterAndStillValid(
  SizingMode sizeMode,
  float size,
  SizingMode lastSizeMode,
  float lastSize,
  float lastComputedSize
) pure {
  return lastSizeMode == SizingMode.FitContent &&
    sizeMode == SizingMode.FitContent && !lastSize.isNaN &&
    !size.isNaN && !lastComputedSize.isNaN &&
    lastSize > size &&
    (lastComputedSize <= size || size.inexactEquals(lastComputedSize));
}

public:
bool canUseCachedMeasurement(
  SizingMode widthMode,
  float availableWidth,
  SizingMode heightMode,
  float availableHeight,
  SizingMode lastWidthMode,
  float lastAvailableWidth,
  SizingMode lastHeightMode,
  float lastAvailableHeight,
  float lastComputedWidth,
  float lastComputedHeight,
  float marginRow,
  float marginColumn,
  const(Config*) config
) pure {
  if (
    (!lastComputedHeight.isNaN && lastComputedHeight < 0) ||
    (!lastComputedWidth.isNaN && lastComputedWidth < 0)
  ) {
    return false;
  }

  float pointScaleFactor = config.getPointScaleFactor;

  bool useRoundedComparison = config!= null && pointScaleFactor != 0;
  float effectiveWidth = useRoundedComparison
    ? roundValueToPixelGrid(
      availableWidth,
      pointScaleFactor,
      false,
      false
    )
    : availableWidth;
  float effectiveHeight = useRoundedComparison
    ? roundValueToPixelGrid(
      availableHeight,
      pointScaleFactor,
      false,
      false
    )
    : availableHeight;
  float effectiveLastWidth = useRoundedComparison
    ? roundValueToPixelGrid(
      lastAvailableWidth,
      pointScaleFactor,
      false,
      false
    )
    : lastAvailableWidth;
  float effectiveLastHeight = useRoundedComparison
    ? roundValueToPixelGrid(
      lastAvailableHeight,
      pointScaleFactor,
      false,
      false
    )
    : lastAvailableHeight;

  bool hasSameWidthSpec = lastWidthMode == widthMode &&
    effectiveLastWidth.inexactEquals(effectiveWidth);
  bool hasSameHeightSpec = lastHeightMode == heightMode &&
    effectiveLastHeight.inexactEquals(effectiveHeight);

  bool widthIsCompatible = hasSameWidthSpec ||
    sizeIsExactAndMatchesOldMeasuredSize(
      widthMode,
      availableWidth - marginRow,
      lastComputedWidth
    ) ||
    oldSizeIsMaxContentAndStillFits(
      widthMode,
      availableWidth - marginRow,
      lastWidthMode,
      lastComputedWidth
    ) ||
    newSizeIsStricterAndStillValid(
      widthMode,
      availableWidth - marginRow,
      lastWidthMode,
      lastAvailableWidth,
      lastComputedWidth
    );

  bool heightIsCompatible = hasSameHeightSpec ||
    sizeIsExactAndMatchesOldMeasuredSize(
      heightMode,
      availableHeight - marginColumn,
      lastComputedHeight
    ) ||
    oldSizeIsMaxContentAndStillFits(
      heightMode,
      availableHeight - marginColumn,
      lastHeightMode,
      lastComputedHeight
    ) ||
    newSizeIsStricterAndStillValid(
      heightMode,
      availableHeight - marginColumn,
      lastHeightMode,
      lastAvailableHeight,
      lastComputedHeight
    );

  return widthIsCompatible && heightIsCompatible;
}