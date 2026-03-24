module yoga.algorithm.flex_line;

import std.range;
import yoga;

struct FlexLineRunningLayout {
  float totalFlexGrowFactors = 0.0f;
  float totalFlexShrinkScaledFactors = 0.0f;
  float remaningFreeSpace = 0.0f;
  float mainDim = 0.0f;
  float crossDim = 0.0f;
}

struct FlexLine {
  Node[] itemsInFlow;
  float sizeConsumed = 0.0f;
  size_t numberOfAutoMargins = 0;
  FlexLineRunningLayout layout;
}

FlexLine calculateFlexLine(
  Node node,
  Direction ownerDirection,
  float ownerWidth,
  float mainAxisOwnerSize,
  float availableInnerWidth,
  float availableInnerMainDim,
  ref LayoutableChildren!Node.Range range,
  size_t lineCount
) {
  Node[] itemsInFlow;
  itemsInFlow.length = node.getChildCount;

  float sizeConsumed = 0.0f;
  float totalFlexGrowFactors = 0.0f;
  float totalFlexShrinkScaledFactors = 0.0f;
  size_t numberOfAutoMargins = 0;
  Node firstElementInLine = null;

  float sizeConsumedIncludingMinConstraint = 0;
  Direction direction = node.inheritDirection(ownerDirection);
  FlexDirection mainAxis = node.style.flexDirection.resolveDirection(direction);
  bool isNodeFlexWrap = node.style.flexWrap != Wrap.NoWrap;
  float gap = node.style.computeGapForAxis(mainAxis, availableInnerMainDim);

  for (; !range.empty; range.popFront()) {
    auto child = range.front;
    if (
      child.style.display == Display.None ||
      child.style.positionType == PositionType.Absolute
    ) {
      continue;
    }

    if (firstElementInLine is null) {
      firstElementInLine = child;
    }

    if (child.style.flexStartMarginIsAuto(mainAxis, ownerDirection)) {
      numberOfAutoMargins++;
    }
    if (child.style.flexEndMarginIsAuto(mainAxis, ownerDirection)) {
      numberOfAutoMargins++;
    }

    child.setLineIndex = lineCount;
    float childMarginMainAxis = child.style.computeMarginForAxis(
      mainAxis, availableInnerWidth);
    float childLeadingGapMainAxis =
      child == firstElementInLine ? 0.0f : gap;
    float flexBasisWithMinAndMaxConstraints = child.boundAxisWithinMinAndMax(
      direction,
      mainAxis,
      child.getLayout.computedFlexBasis,
      mainAxisOwnerSize,
      ownerWidth
    );

    if (
      sizeConsumedIncludingMinConstraint +
      flexBasisWithMinAndMaxConstraints +
      childMarginMainAxis +
      childLeadingGapMainAxis >
      availableInnerMainDim &&
      isNodeFlexWrap &&
      !itemsInFlow.empty
    ) {
      break;
    }

    sizeConsumedIncludingMinConstraint += flexBasisWithMinAndMaxConstraints +
      childMarginMainAxis + childLeadingGapMainAxis;
    sizeConsumed += flexBasisWithMinAndMaxConstraints + childMarginMainAxis +
      childLeadingGapMainAxis;

    if (child.isNodeFlexible) {
      totalFlexGrowFactors += child.resolveFlexGrow;

      totalFlexShrinkScaledFactors += child.resolveFlexShrink +
        child.getLayout.computedFlexBasis;
    }

    itemsInFlow ~= child;
  }

  if (totalFlexGrowFactors > 0 && totalFlexGrowFactors < 1) {
    totalFlexGrowFactors = 1;
  }

  if (totalFlexShrinkScaledFactors > 0 && totalFlexShrinkScaledFactors < 1) {
    totalFlexShrinkScaledFactors = 1;
  }

  FlexLine ret;
  ret.itemsInFlow = itemsInFlow;
  ret.sizeConsumed = sizeConsumed;
  ret.numberOfAutoMargins = numberOfAutoMargins;
  ret.layout = FlexLineRunningLayout(
    totalFlexGrowFactors,
    totalFlexShrinkScaledFactors
  );

  return ret;
}