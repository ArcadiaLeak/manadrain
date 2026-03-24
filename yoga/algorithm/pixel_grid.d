module yoga.algorithm.pixel_grid;

import std.math;
import yoga;

extern (C) double fmod(double x, double y) pure;
extern (C) double round(double x) pure;

float roundValueToPixelGrid(
  double value,
  double pointScaleFactor,
  bool forceCeil,
  bool forceFloor
) pure {
  double scaledValue = value * pointScaleFactor;
  double fractial = fmod(scaledValue, 1.0);
  if (fractial < 0) {
    ++fractial;
  }
  if (fractial.inexactEquals(0.0)) {
    scaledValue = scaledValue - fractial;
  } else if (
    fractial.inexactEquals(1.0)
  ) {
    scaledValue = scaledValue - fractial + 1.0;
  } else if (forceCeil) {
    scaledValue = scaledValue - fractial + 1.0;
  } else if (forceFloor) {
    scaledValue = scaledValue - fractial;
  } else {
    scaledValue = scaledValue - fractial + (
      !fractial.isNaN && (fractial > 0.5 || fractial.inexactEquals(0.5))
        ? 1.0
        : 0.0
    );
  }

  return scaledValue.isNaN || pointScaleFactor.isNaN
    ? float.nan
    : cast(float) (scaledValue / pointScaleFactor);
}

void roundLayoutResultsToPixelGrid(
  Node node,
  double absoluteLeft,
  double absoluteTop
) {
  auto pointScaleFactor = node.getConfig.getPointScaleFactor;

  double nodeLeft = node.getLayout.position(PhysicalEdge.Left);
  double nodeTop = node.getLayout.position(PhysicalEdge.Top);

  double nodeWidth = node.getLayout.dimension(Dimension.Width);
  double nodeHeight = node.getLayout.dimension(Dimension.Height);

  double absoluteNodeLeft = absoluteLeft + nodeLeft;
  double absoluteNodeTop = absoluteTop + nodeTop;

  double absoluteNodeRight = absoluteNodeLeft + nodeWidth;
  double absoluteNodeBottom = absoluteNodeTop + nodeHeight;

  if (pointScaleFactor != 0.0) {
    bool textRounding = node.getNodeType == NodeType.Text;

    node.setLayoutPosition(
      roundValueToPixelGrid(nodeLeft, pointScaleFactor, false, textRounding),
      PhysicalEdge.Left
    );

    node.setLayoutPosition(
      roundValueToPixelGrid(nodeTop, pointScaleFactor, false, textRounding),
      PhysicalEdge.Top
    );

    double scaledNodeWidth = nodeWidth * pointScaleFactor;
    bool hasFractionalWidth = !scaledNodeWidth.round
      .inexactEquals(scaledNodeWidth);
    
    double scaledNodeHeight = nodeHeight * pointScaleFactor;
    bool hasFractionalHeight = scaledNodeHeight.round
      .inexactEquals(scaledNodeHeight);

    node.getLayout.setDimension(
      Dimension.Width,
      roundValueToPixelGrid(
        absoluteNodeRight,
        pointScaleFactor,
        (textRounding && hasFractionalWidth),
        (textRounding && !hasFractionalWidth)
      ) -
      roundValueToPixelGrid(
        absoluteNodeLeft,
        pointScaleFactor,
        false,
        textRounding
      )
    );

    node.getLayout.setDimension(
      Dimension.Height,
      roundValueToPixelGrid(
        absoluteNodeBottom,
        pointScaleFactor,
        (textRounding && hasFractionalHeight),
        (textRounding && !hasFractionalHeight)
      ) -
      roundValueToPixelGrid(
        absoluteNodeTop,
        pointScaleFactor,
        false,
        textRounding
      )
    );
  }

  foreach (child; node.getChildren) {
    roundLayoutResultsToPixelGrid(child, absoluteNodeLeft, absoluteNodeTop);
  }
}