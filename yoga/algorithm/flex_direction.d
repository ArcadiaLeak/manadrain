module yoga.algorithm.flex_direction;

import yoga;

bool isRow(FlexDirection flexDirection) pure {
  return flexDirection == FlexDirection.Row ||
    flexDirection == FlexDirection.RowReverse;
}

bool isColumn(FlexDirection flexDirection) pure {
  return flexDirection == FlexDirection.Column ||
    flexDirection == FlexDirection.ColumnReverse;
}

FlexDirection resolveDirection(
  const FlexDirection flexDirection,
  const Direction direction
) {
  if (direction == Direction.RTL) {
    if (flexDirection == FlexDirection.Row) {
      return FlexDirection.RowReverse;
    } else if (flexDirection == FlexDirection.RowReverse) {
      return FlexDirection.Row;
    }
  }

  return flexDirection;
}

FlexDirection resolveCrossDirection(
  const FlexDirection flexDirection,
  const Direction direction
) {
  return isColumn(flexDirection)
    ? resolveDirection(FlexDirection.Row, direction)
    : FlexDirection.Column;
}

PhysicalEdge flexStartEdge(FlexDirection flexDirection) pure {
  final switch (flexDirection) {
    case FlexDirection.Column:
      return PhysicalEdge.Top;
    case FlexDirection.ColumnReverse:
      return PhysicalEdge.Bottom;
    case FlexDirection.Row:
      return PhysicalEdge.Left;
    case FlexDirection.RowReverse:
      return PhysicalEdge.Right;
  }
}

PhysicalEdge flexEndEdge(FlexDirection flexDirection) pure {
  final switch (flexDirection) {
    case FlexDirection.Column:
      return PhysicalEdge.Bottom;
    case FlexDirection.ColumnReverse:
      return PhysicalEdge.Top;
    case FlexDirection.Row:
      return PhysicalEdge.Right;
    case FlexDirection.RowReverse:
      return PhysicalEdge.Left;
  }
}

PhysicalEdge inlineStartEdge(
  FlexDirection flexDirection,
  Direction direction
) pure {
  if (isRow(flexDirection)) {
    return direction == Direction.RTL
      ? PhysicalEdge.Right
      : PhysicalEdge.Left;
  }

  return PhysicalEdge.Top;
}

PhysicalEdge inlineEndEdge(
  FlexDirection flexDirection,
  Direction direction
) pure {
  if (isRow(flexDirection)) {
    return direction == Direction.RTL
      ? PhysicalEdge.Left
      : PhysicalEdge.Right;
  }

  return PhysicalEdge.Bottom;
}

Dimension dimension(FlexDirection flexDirection) pure {
  final switch (flexDirection) {
    case FlexDirection.Column:
      return Dimension.Height;
    case FlexDirection.ColumnReverse:
      return Dimension.Height;
    case FlexDirection.Row:
      return Dimension.Width;
    case FlexDirection.RowReverse:
      return Dimension.Width;
  }
}
