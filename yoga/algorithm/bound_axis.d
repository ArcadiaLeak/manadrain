module yoga.algorithm.bound_axis;

import yoga;

float paddingAndBorderForAxis(
  Node node,
  FlexDirection axis,
  Direction direction,
  float widthSize
) pure {
  return
    node.style.computeInlineStartPaddingAndBorder(
      axis, direction, widthSize
    ) +
    node.style.computeInlineEndPaddingAndBorder(
      axis, direction, widthSize
    );
}

FloatOptional boundAxisWithinMinAndMax(
  Node node,
  Direction direction,
  FlexDirection axis,
  FloatOptional value,
  float axisSize,
  float widthSize
) pure {
  FloatOptional min;
  FloatOptional max;

  if (axis.isColumn) {
    min = node.style.resolvedMinDimension(
      direction,
      Dimension.Height,
      axisSize,
      widthSize
    );
    max = node.style.resolvedMaxDimension(
      direction,
      Dimension.Height,
      axisSize,
      widthSize
    );
  } else if (axis.isRow) {
    min = node.style.resolvedMinDimension(
      direction,
      Dimension.Width,
      axisSize,
      widthSize
    );
    max = node.style.resolvedMaxDimension(
      direction,
      Dimension.Width,
      axisSize,
      widthSize
    );
  }

  if (max >= FloatOptional(0) && value > max) {
    return max;
  }

  if (min >= FloatOptional(0) && value < min) {
    return min;
  }

  return value;
}

float boundAxis(
  Node node,
  FlexDirection axis,
  Direction direction,
  float value,
  float axisSize,
  float widthSize
) pure {
  return maxOrDefined(
    boundAxisWithinMinAndMax(
      node,
      direction,
      axis,
      FloatOptional(value),
      axisSize,
      widthSize
    ),
    paddingAndBorderForAxis(
      node,
      axis,
      direction,
      widthSize
    )
  );
}