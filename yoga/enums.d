module yoga.enums;

enum Align : ubyte {
  Auto,
  FlexStart,
  Center,
  FlexEnd,
  Stretch,
  Baseline,
  SpaceBetween,
  SpaceAround,
  SpaceEvenly,
}

enum Unit : ubyte {
  Undefined,
  Point,
  Percent,
  Auto,
  MaxContent,
  FitContent,
  Stretch,
}

enum BoxSizing : ubyte {
  BorderBox,
  ContentBox,
}

enum Dimension : ubyte {
  Width,
  Height,
}

enum Direction : ubyte {
  Inherit,
  LTR,
  RTL,
}

enum Display : ubyte {
  Flex,
  None,
  Contents,
}

enum Edge : ubyte {
  Left,
  Top,
  Right,
  Bottom,
  Start,
  End,
  Horizontal,
  Vertical,
  All,
}

enum FlexDirection : ubyte {
  Column,
  ColumnReverse,
  Row,
  RowReverse,
}

enum Gutter : ubyte {
  Column,
  Row,
  All,
}

enum Justify : ubyte {
  FlexStart,
  Center,
  FlexEnd,
  SpaceBetween,
  SpaceAround,
  SpaceEvenly,
}

enum MeasureMode : ubyte {
  Undefined,
  Exactly,
  AtMost,
}

enum NodeType : ubyte {
  Default,
  Text,
}

enum Overflow : ubyte {
  Visible,
  Hidden,
  Scroll,
}

enum PhysicalEdge : ubyte {
  Left,
  Top,
  Right,
  Bottom,
}

enum PositionType : ubyte {
  Static,
  Relative,
  Absolute,
}

enum Wrap : ubyte {
  NoWrap,
  Wrap,
  WrapReverse,
}
