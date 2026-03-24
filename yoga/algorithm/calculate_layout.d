module yoga.algorithm.calculate_layout;

import core.atomic;
import core.builtins;
import std.algorithm;
import std.math;
import yoga;

private shared uint gCurrentGenerationCount = 0;

void calculateLayout(
  Node node,
  float ownerWidth,
  float ownerHeight,
  Direction ownerDirection
) {
  Event.publish!(Event.Type.LayoutPassStart)(node);
  LayoutData markerData;

  gCurrentGenerationCount.atomicFetchAdd!(MemoryOrder.raw)(1);
  node.processDimensions();
  Direction direction = node.inheritDirection(ownerDirection);
  float width = float.nan;
  SizingMode widthSizingMode = SizingMode.MaxContent;
  ref Style style = node.style();
  if (node.hasDefiniteLength(Dimension.Width, ownerWidth)) {
    width = node.getResolvedDimension(
      direction,
      dimension(FlexDirection.Row),
      ownerWidth,
      ownerWidth
    ) + node.style.computeMarginForAxis(
      FlexDirection.Row,
      ownerWidth
    );
    widthSizingMode = SizingMode.StretchFit;
  } else if (
    !style
      .resolvedMaxDimension(direction, Dimension.Width, ownerWidth, ownerWidth)
      .isNull
  ) {
    width = style
      .resolvedMaxDimension(direction, Dimension.Width, ownerWidth, ownerWidth);
    widthSizingMode = SizingMode.FitContent;
  } else {
    width = ownerWidth;
    widthSizingMode = width.isNaN
      ? SizingMode.MaxContent
      : SizingMode.StretchFit;
  }

  float height = float.nan;
  SizingMode heightSizingMode = SizingMode.MaxContent;
  if (node.hasDefiniteLength(Dimension.Height, ownerHeight)) {
    height = node.getResolvedDimension(
      direction,
      dimension(FlexDirection.Column),
      ownerHeight,
      ownerWidth
    ) + node.style.computeMarginForAxis(
      FlexDirection.Column,
      ownerWidth
    );
    heightSizingMode = SizingMode.StretchFit;
  } else if (
    !style
      .resolvedMaxDimension(direction, Dimension.Height, ownerHeight, ownerWidth)
      .isNull
  ) {
    height = style
      .resolvedMaxDimension(direction, Dimension.Height, ownerHeight, ownerWidth);
    heightSizingMode = height.isNaN
      ? SizingMode.MaxContent
      : SizingMode.StretchFit;
  }
  if (
    node.calculateLayoutInternal(
      width,
      height,
      ownerDirection,
      widthSizingMode,
      heightSizingMode,
      ownerWidth,
      ownerHeight,
      true,
      LayoutPassReason.kInitial,
      markerData,
      0,
      gCurrentGenerationCount.atomicLoad!(MemoryOrder.raw)
    )
  ) {
    node.setPosition(
      node.getLayout().direction,
      ownerWidth,
      ownerHeight
    );
    node.roundLayoutResultsToPixelGrid(0.0, 0.0);
  }

  Event.publish!(Event.Type.LayoutPassEnd)(
    node,
    Event.TypedData!(Event.Type.LayoutPassEnd)(&markerData)
  );
}

bool calculateLayoutInternal(
  Node node,
  float availableWidth,
  float availableHeight,
  Direction ownerDirection,
  SizingMode widthSizingMode,
  SizingMode heightSizingMode,
  float ownerWidth,
  float ownerHeight,
  bool performLayout,
  LayoutPassReason reason,
  ref LayoutData layoutMarkerData,
  uint depth,
  uint generationCount
) {
  ref LayoutResults layout = node.getLayout();

  depth++;

  bool needToVisitNode =
    (node.isDirty() && layout.generationCount != generationCount) ||
    layout.lastOwnerDirection != ownerDirection;

  if (needToVisitNode) {
    layout.nextCachedMeasurementsIndex = 0;
    layout.cachedLayout.availableWidth = -1;
    layout.cachedLayout.availableHeight = -1;
    layout.cachedLayout.widthSizingMode = SizingMode.MaxContent;
    layout.cachedLayout.heightSizingMode = SizingMode.MaxContent;
    layout.cachedLayout.computedWidth = -1;
    layout.cachedLayout.computedHeight = -1;
  }

  CachedMeasurement* cachedResults = null;

  if (node.hasMeasureFunc) {
    float marginAxisRow = node.style
      .computeMarginForAxis(FlexDirection.Row, ownerWidth);
    float marginAxisColumn = node.style
      .computeMarginForAxis(FlexDirection.Column, ownerWidth);

    if (
      canUseCachedMeasurement(
        widthSizingMode,
        availableWidth,
        heightSizingMode,
        availableHeight,
        layout.cachedLayout.widthSizingMode,
        layout.cachedLayout.availableWidth,
        layout.cachedLayout.heightSizingMode,
        layout.cachedLayout.availableHeight,
        layout.cachedLayout.computedWidth,
        layout.cachedLayout.computedHeight,
        marginAxisRow,
        marginAxisColumn,
        node.getConfig
      )
    ) {
      cachedResults = &layout.cachedLayout;
    } else {
      for (size_t i = 0; i < layout.nextCachedMeasurementsIndex; i++) {
        if (
          canUseCachedMeasurement(
            widthSizingMode,
            availableWidth,
            heightSizingMode,
            availableHeight,
            layout.cachedMeasurements[i].widthSizingMode,
            layout.cachedMeasurements[i].availableWidth,
            layout.cachedMeasurements[i].heightSizingMode,
            layout.cachedMeasurements[i].availableHeight,
            layout.cachedMeasurements[i].computedWidth,
            layout.cachedMeasurements[i].computedHeight,
            marginAxisRow,
            marginAxisColumn,
            node.getConfig
          )
        ) {
          cachedResults = &layout.cachedMeasurements[i];
          break;
        }
      }
    }
  } else if (performLayout) {
    if (
      layout.cachedLayout.availableWidth.inexactEquals(availableWidth) &&
      layout.cachedLayout.availableHeight.inexactEquals(availableHeight) &&
      layout.cachedLayout.widthSizingMode == widthSizingMode &&
      layout.cachedLayout.heightSizingMode == heightSizingMode
    ) {
      cachedResults = &layout.cachedLayout;
    }
  } else {
    for (uint i = 0; i < layout.nextCachedMeasurementsIndex; i++) {
      if (
        layout.cachedMeasurements[i].availableWidth
          .inexactEquals(availableWidth) &&
        layout.cachedMeasurements[i].availableHeight
          .inexactEquals(availableHeight) &&
        layout.cachedMeasurements[i].widthSizingMode ==
          widthSizingMode &&
        layout.cachedMeasurements[i].heightSizingMode ==
          heightSizingMode
      ) {
        cachedResults = &layout.cachedMeasurements[i];
        break;
      }
    }
  }

  if (!needToVisitNode && cachedResults !is null) {
    layout.setMeasuredDimension(
      Dimension.Width,
      cachedResults.computedWidth
    );
    layout.setMeasuredDimension(
      Dimension.Height,
      cachedResults.computedHeight
    );

    (performLayout ? layoutMarkerData.cachedLayouts
      : layoutMarkerData.cachedMeasures) += 1;
  } else {
    node.calculateLayoutImpl(
      availableWidth,
      availableHeight,
      ownerDirection,
      widthSizingMode,
      heightSizingMode,
      ownerWidth,
      ownerHeight,
      performLayout,
      reason,
      layoutMarkerData,
      depth,
      generationCount
    );

    layout.lastOwnerDirection = ownerDirection;

    if (cachedResults is null) {
      layoutMarkerData.maxMeasureCache = max(
        layoutMarkerData.maxMeasureCache,
        layout.nextCachedMeasurementsIndex + 1u
      );

      if (
        layout.nextCachedMeasurementsIndex ==
        LayoutResults.MaxCachedMeasurements
      ) {
        layout.nextCachedMeasurementsIndex = 0;
      }

      CachedMeasurement* newCacheEntry = null;
      if (performLayout) {
        newCacheEntry = &layout.cachedLayout;
      } else {
        newCacheEntry = &layout.cachedMeasurements[
          layout.nextCachedMeasurementsIndex
        ];
      }

      newCacheEntry.availableWidth = availableWidth;
      newCacheEntry.availableHeight = availableHeight;
      newCacheEntry.widthSizingMode = widthSizingMode;
      newCacheEntry.heightSizingMode = heightSizingMode;
      newCacheEntry.computedWidth =
        layout.measuredDimension(Dimension.Width);
      newCacheEntry.computedHeight =
        layout.measuredDimension(Dimension.Height);
    }
  }

  if (performLayout) {
    node.setLayoutDimension(
      node.getLayout.measuredDimension(Dimension.Width),
      Dimension.Width
    );
    node.setLayoutDimension(
      node.getLayout.measuredDimension(Dimension.Height),
      Dimension.Height
    );

    node.setHasNewLayout = true;
    node.setDirty = false;
  }

  layout.generationCount = generationCount;

  LayoutType layoutType;
  if (performLayout) {
    layoutType = !needToVisitNode && cachedResults == &layout.cachedLayout
      ? LayoutType.kCachedLayout
      : LayoutType.kLayout;
  } else {
    layoutType = cachedResults !is null
      ? LayoutType.kCachedMeasure
      : LayoutType.kMeasure;
  }
  Event.publish!(Event.Type.NodeLayout)(
    node,
    Event.TypedData!(Event.Type.NodeLayout)(
      layoutType
    )
  );

  return (needToVisitNode || cachedResults is null);
}

private void measureNodeWithMeasureFunc(
  Node node,
  Direction direction,
  float availableWidth,
  float availableHeight,
  SizingMode widthSizingMode,
  SizingMode heightSizingMode,
  float ownerWidth,
  float ownerHeight,
  ref LayoutData layoutMarkerData,
  LayoutPassReason reason
)
in {
  assert(
    node.hasMeasureFunc(),
    "Expected node to have custom measure function"
  );
}
do {
  if (widthSizingMode == SizingMode.MaxContent) {
    availableWidth = float.nan;
  }
  if (heightSizingMode == SizingMode.MaxContent) {
    availableHeight = float.nan;
  }

  auto ref layout = node.getLayout();
  float paddingAndBorderAxisRow =
    layout.padding(PhysicalEdge.Left) +
    layout.padding(PhysicalEdge.Right) +
    layout.border(PhysicalEdge.Left) +
    layout.border(PhysicalEdge.Right);
  float paddingAndBorderAxisColumn =
    layout.padding(PhysicalEdge.Top) +
    layout.padding(PhysicalEdge.Bottom) +
    layout.border(PhysicalEdge.Top) +
    layout.border(PhysicalEdge.Bottom);

  float innerWidth = availableWidth.isNaN
    ? availableWidth
    : 0.0f.maxOrDefined(availableWidth - paddingAndBorderAxisRow);
  float innerHeight = availableHeight.isNaN
    ? availableHeight
    : 0.0f.maxOrDefined(availableHeight - paddingAndBorderAxisColumn);

  if (
    widthSizingMode == SizingMode.StretchFit &&
    heightSizingMode == SizingMode.StretchFit
  ) {
    node.setLayoutMeasuredDimension(
      node.boundAxis(
        FlexDirection.Row,
        direction,
        availableWidth,
        ownerWidth,
        ownerWidth
      ),
      Dimension.Width
    );
    node.setLayoutMeasuredDimension(
      node.boundAxis(
        FlexDirection.Column,
        direction,
        availableHeight,
        ownerHeight,
        ownerWidth
      ),
      Dimension.Height
    );
  } else {
    Event.publish!(Event.Type.MeasureCallbackStart)(node);

    const YGSize measuredSize = node.measure(
      innerWidth,
      widthSizingMode.measureMode,
      innerHeight,
      heightSizingMode.measureMode,
    );

    layoutMarkerData.measureCallbacks += 1;
    layoutMarkerData.measureCallbackReasonsCount[reason] += 1;
    
    Event.publish!(Event.Type.MeasureCallbackEnd)(
      node,
      Event.TypedData!(Event.Type.MeasureCallbackEnd)(
        innerWidth,
        widthSizingMode.measureMode,
        innerHeight,
        heightSizingMode.measureMode,
        measuredSize.width,
        measuredSize.height,
        reason
      )
    );

    node.setLayoutMeasuredDimension(
      boundAxis(
        node,
        FlexDirection.Row,
        direction,
        (widthSizingMode == SizingMode.MaxContent ||
          widthSizingMode == SizingMode.FitContent)
            ? measuredSize.width + paddingAndBorderAxisRow
            : availableWidth,
        ownerWidth,
        ownerWidth
      ),
      Dimension.Width
    );

    node.setLayoutMeasuredDimension(
      boundAxis(
        node,
        FlexDirection.Column,
        direction,
        (heightSizingMode == SizingMode.MaxContent ||
          heightSizingMode == SizingMode.FitContent)
            ? measuredSize.height + paddingAndBorderAxisColumn
            : availableHeight,
        ownerHeight,
        ownerWidth
      ),
      Dimension.Height
    );
  }
}

private void calculateLayoutImpl(
  Node node,
  float availableWidth,
  float availableHeight,
  Direction ownerDirection,
  SizingMode widthSizingMode,
  SizingMode heightSizingMode,
  float ownerWidth,
  float ownerHeight,
  bool performLayout,
  LayoutPassReason reason,
  ref LayoutData layoutMarkerData,
  uint depth,
  uint generationCount
)
in {
  assert(
    availableWidth.isNaN
      ? widthSizingMode == SizingMode.MaxContent
      : true,
    "availableWidth is indefinite so widthSizingMode must be " ~
    "SizingMode.MaxContent"
  );
  assert(
    availableHeight.isNaN
      ? heightSizingMode == SizingMode.MaxContent
      : true,
    "availableHeight is indefinite so heightSizingMode must be " ~
    "SizingMode.MaxContent"
  );
}
do {
  (performLayout ? layoutMarkerData.layouts
    : layoutMarkerData.measures) += 1;

  Direction direction = node.inheritDirection(ownerDirection);
  node.setLayoutDirection = direction;

  FlexDirection flexRowDirection = resolveDirection(
    FlexDirection.Row,
    direction
  );
  FlexDirection flexColumnDirection = resolveDirection(
    FlexDirection.Column,
    direction
  );

  auto startEdge = direction == Direction.LTR
    ? PhysicalEdge.Left : PhysicalEdge.Right;
  auto endEdge = direction == Direction.LTR
    ? PhysicalEdge.Right : PhysicalEdge.Left;

  float marginRowLeading = node.style.computeInlineStartMargin(
    flexRowDirection, direction, ownerWidth
  );
  node.setLayoutMargin(marginRowLeading, startEdge);
  float marginRowTrailing = node.style.computeInlineEndMargin(
    flexRowDirection, direction, ownerWidth
  );
  node.setLayoutMargin(marginRowTrailing, endEdge);
  float marginColumnLeading = node.style.computeInlineStartMargin(
    flexColumnDirection, direction, ownerWidth
  );
  node.setLayoutMargin(marginColumnLeading, PhysicalEdge.Top);
  float marginColumnTrailing = node.style.computeInlineEndMargin(
    flexColumnDirection, direction, ownerWidth
  );
  node.setLayoutMargin(marginColumnTrailing, PhysicalEdge.Bottom);

  float marginAxisRow = marginRowLeading + marginRowTrailing;
  float marginAxisColumn = marginColumnLeading + marginColumnTrailing;

  node.setLayoutBorder(
    node.style.computeInlineStartBorder(flexRowDirection, direction),
    startEdge
  );
  node.setLayoutBorder(
    node.style.computeInlineEndBorder(flexRowDirection, direction),
    endEdge
  );
  node.setLayoutBorder(
    node.style.computeInlineStartBorder(flexColumnDirection, direction),
    PhysicalEdge.Top
  );
  node.setLayoutBorder(
    node.style.computeInlineEndBorder(flexColumnDirection, direction),
    PhysicalEdge.Bottom
  );

  node.setLayoutPadding(
    node.style.computeInlineStartPadding(
      flexRowDirection, direction, ownerWidth
    ),
    startEdge
  );
  node.setLayoutPadding(
    node.style.computeInlineEndPadding(
      flexRowDirection, direction, ownerWidth
    ),
    endEdge
  );
  node.setLayoutPadding(
    node.style.computeInlineStartPadding(
      flexColumnDirection, direction, ownerWidth
    ),
    PhysicalEdge.Top
  );
  node.setLayoutPadding(
    node.style.computeInlineEndPadding(
      flexColumnDirection, direction, ownerWidth
    ),
    PhysicalEdge.Bottom
  );

  if (node.hasMeasureFunc) {
    node.measureNodeWithMeasureFunc(
      direction,
      availableWidth - marginAxisRow,
      availableHeight - marginAxisColumn,
      widthSizingMode,
      heightSizingMode,
      ownerWidth,
      ownerHeight,
      layoutMarkerData,
      reason
    );

    cleanupContentsNodesRecursively(node);
    return;
  }

  auto childCount = node.getLayoutChildCount();
  if (childCount == 0) {
    node.measureNodeWithoutChildren(
      direction,
      availableWidth - marginAxisRow,
      availableHeight - marginAxisColumn,
      widthSizingMode,
      heightSizingMode,
      ownerWidth,
      ownerHeight
    );

    cleanupContentsNodesRecursively(node);
    return;
  }

  if (
    !performLayout &&
    node.measureNodeWithFixedSize(
      direction,
      availableWidth - marginAxisRow,
      availableHeight - marginAxisColumn,
      widthSizingMode,
      heightSizingMode,
      ownerWidth,
      ownerHeight
    )
  ) {
    cleanupContentsNodesRecursively(node);
    return;
  }

  node.cloneChildrenIfNeeded();
  node.setLayoutHadOverflow = false;

  cleanupContentsNodesRecursively(node);

  FlexDirection mainAxis = resolveDirection(
    node.style.flexDirection,
    direction
  );
  FlexDirection crossAxis = resolveCrossDirection(
    mainAxis,
    direction
  );
  bool isMainAxisRow = mainAxis.isRow;
  bool isNodeFlexWrap = node.style.flexWrap != Wrap.NoWrap;

  float mainAxisOwnerSize = isMainAxisRow ? ownerWidth : ownerHeight;
  float crossAxisOwnerSize = isMainAxisRow ? ownerHeight : ownerWidth;

  float paddingAndBorderAxisMain = paddingAndBorderForAxis(
    node,
    mainAxis,
    direction,
    ownerWidth
  );
  float paddingAndBorderAxisCross = paddingAndBorderForAxis(
    node,
    crossAxis,
    direction,
    ownerWidth
  );
  float leadingPaddingAndBorderCross =
    node.style.computeFlexStartPaddingAndBorder(
      crossAxis, direction, ownerWidth);

  SizingMode sizingModeMainDim = isMainAxisRow
    ? widthSizingMode : heightSizingMode;
  SizingMode sizingModeCrossDim = isMainAxisRow
    ? heightSizingMode : widthSizingMode;

  float paddingAndBorderAxisRow = isMainAxisRow
    ? paddingAndBorderAxisMain : paddingAndBorderAxisCross;
  float paddingAndBorderAxisColumn = isMainAxisRow
    ? paddingAndBorderAxisCross : paddingAndBorderAxisMain;

  float availableInnerWidth = calculateAvailableInnerDimension(
    node,
    direction,
    Dimension.Width,
    availableWidth - marginAxisRow,
    paddingAndBorderAxisRow,
    ownerWidth,
    ownerWidth
  );
  float availableInnerHeight = calculateAvailableInnerDimension(
    node,
    direction,
    Dimension.Height,
    availableHeight - marginAxisColumn,
    paddingAndBorderAxisColumn,
    ownerHeight,
    ownerWidth
  );

  float availableInnerMainDim = isMainAxisRow
    ? availableInnerWidth : availableInnerHeight;
  float availableInnerCrossDim = isMainAxisRow
    ? availableInnerHeight : availableInnerWidth;

  float totalMainDim = 0;
  totalMainDim += node.computeFlexBasisForChildren(
    availableInnerWidth,
    availableInnerHeight,
    widthSizingMode,
    heightSizingMode,
    direction,
    mainAxis,
    performLayout,
    layoutMarkerData,
    depth,
    generationCount
  );

  if (childCount > 1) {
    totalMainDim += node.style.computeGapForAxis(
      mainAxis, availableInnerMainDim) * cast(float) (childCount - 1);
  }

  bool mainAxisOverflows =
    (sizingModeMainDim != SizingMode.MaxContent) &&
    totalMainDim > availableInnerMainDim;

  if (isNodeFlexWrap && mainAxisOverflows &&
    sizingModeMainDim == SizingMode.FitContent) {
    sizingModeMainDim = SizingMode.StretchFit;
  }

  LayoutableChildren!Node.Range startOfLineRange =
    node.getLayoutChildren[];

  size_t lineCount = 0;

  float totalLineCrossDim = 0;

  float crossAxisGap = node.style.computeGapForAxis(
    crossAxis, availableInnerCrossDim);

  float maxLineMainDim = 0;
  for (; startOfLineRange.empty != true; lineCount++) {
    auto flexLine = node.calculateFlexLine(
      ownerDirection,
      ownerWidth,
      mainAxisOwnerSize,
      availableInnerWidth,
      availableInnerMainDim,
      startOfLineRange,
      lineCount
    );

    bool canSkipFlex = !performLayout &&
      sizingModeCrossDim == SizingMode.StretchFit;

    bool sizeBasedOnContent = false;
    
    if (sizingModeMainDim != SizingMode.StretchFit) {
      auto ref style = node.style;
      float minInnerWidth = style.resolvedMinDimension(
        direction, Dimension.Width, ownerWidth, ownerWidth) -
        paddingAndBorderAxisRow;
      float maxInnerWidth = style.resolvedMaxDimension(
        direction, Dimension.Width, ownerWidth, ownerWidth) -
        paddingAndBorderAxisRow;
      float minInnerHeight = style.resolvedMinDimension(
        direction, Dimension.Height, ownerHeight, ownerWidth) -
        paddingAndBorderAxisColumn;
      float maxInnerHeight = style.resolvedMaxDimension(
        direction, Dimension.Height, ownerHeight, ownerWidth) -
        paddingAndBorderAxisColumn;

      float minInnerMainDim =
        isMainAxisRow ? minInnerWidth : minInnerHeight;
      float maxInnerMainDim =
        isMainAxisRow ? maxInnerWidth : maxInnerHeight;

      if (
        !minInnerMainDim.isNaN &&
        flexLine.sizeConsumed < minInnerMainDim
      ) {
        availableInnerMainDim = minInnerMainDim;
      } else if (
        !maxInnerMainDim.isNaN &&
        flexLine.sizeConsumed > maxInnerMainDim
      ) {
        availableInnerMainDim = maxInnerMainDim;
      } else {
        if (
          (!flexLine.layout.totalFlexGrowFactors.isNaN &&
            flexLine.layout.totalFlexGrowFactors == 0) ||
          (!node.resolveFlexGrow.isNaN &&
            node.resolveFlexGrow == 0)
        ) {
          availableInnerMainDim = flexLine.sizeConsumed;
        }
        sizeBasedOnContent = true;
      }
    }

    
  }
}

private void cleanupContentsNodesRecursively(Node node) {
  if (node.hasContentsChildren.unlikely) {
    node.cloneContentsChildrenIfNeeded();
    foreach(child; node.getChildren) {
      if (child.style.display == Display.Contents) {
        child.getLayout = LayoutResults();
        child.setLayoutDimension(0, Dimension.Width);
        child.setLayoutDimension(0, Dimension.Height);
        child.setHasNewLayout = true;
        child.setDirty = false;
        child.cloneChildrenIfNeeded();

        child.cleanupContentsNodesRecursively();
      }
    }
  }
}

private void measureNodeWithoutChildren(
  Node node,
  Direction direction,
  float availableWidth,
  float availableHeight,
  SizingMode widthSizingMode,
  SizingMode heightSizingMode,
  float ownerWidth,
  float ownerHeight
) {
  auto ref layout = node.getLayout();

  float width = availableWidth;
  if (
    widthSizingMode == SizingMode.MaxContent ||
    widthSizingMode == SizingMode.FitContent
  ) {
    width = layout.padding(PhysicalEdge.Left) +
      layout.padding(PhysicalEdge.Right) +
      layout.border(PhysicalEdge.Left) +
      layout.border(PhysicalEdge.Right);
  }
  node.setLayoutMeasuredDimension(
    node.boundAxis(
      FlexDirection.Row,
      direction,
      width,
      ownerWidth,
      ownerWidth
    ),
    Dimension.Width
  );

  float height = availableHeight;
  if (
    heightSizingMode == SizingMode.MaxContent ||
    heightSizingMode == SizingMode.FitContent
  ) {
    height = layout.padding(PhysicalEdge.Top) +
      layout.padding(PhysicalEdge.Bottom) +
      layout.border(PhysicalEdge.Top) +
      layout.border(PhysicalEdge.Bottom);
  }
  node.setLayoutMeasuredDimension(
    node.boundAxis(
      FlexDirection.Column,
      direction,
      height,
      ownerHeight,
      ownerWidth
    ),
    Dimension.Height
  );
}

private bool isFixedSize(
  float dim,
  SizingMode sizingMode
) {
  return sizingMode == SizingMode.StretchFit ||
    (!dim.isNaN && sizingMode == SizingMode.FitContent &&
      dim <= 0.0);
}

private bool measureNodeWithFixedSize(
  Node node,
  Direction direction,
  float availableWidth,
  float availableHeight,
  SizingMode widthSizingMode,
  SizingMode heightSizingMode,
  float ownerWidth,
  float ownerHeight
) {
  if (
    isFixedSize(availableWidth, widthSizingMode) &&
    isFixedSize(availableHeight, heightSizingMode)
  ) {
    node.setLayoutMeasuredDimension(
      node.boundAxis(
        FlexDirection.Row,
        direction,
        availableWidth.isNaN ||
          (widthSizingMode == SizingMode.FitContent &&
            availableWidth < 0.0f)
          ? 0.0f
          : availableWidth,
        ownerWidth,
        ownerWidth
      ),
      Dimension.Width
    );

    node.setLayoutMeasuredDimension(
      node.boundAxis(
        FlexDirection.Column,
        direction,
        availableHeight.isNaN ||
          (heightSizingMode == SizingMode.FitContent &&
            availableHeight < 0.0f)
          ? 0.0f
          : availableHeight,
        ownerHeight,
        ownerWidth
      ),
      Dimension.Height
    );
    return true;
  }

  return false;
}

private float calculateAvailableInnerDimension(
  Node node,
  Direction direction,
  Dimension dimension,
  float availableDim,
  float paddingAndBorder,
  float ownerDim,
  float ownerWidth
) {
  float availableInnerDim = availableDim - paddingAndBorder;
  
  if (!availableInnerDim.isNaN) {
    FloatOptional minDimensionOptional = node.style.resolvedMinDimension(
      direction, dimension, ownerDim, ownerWidth);
    float minInnerDim = minDimensionOptional.isNull
      ? 0.0f : minDimensionOptional - paddingAndBorder;

    FloatOptional maxDimensionOptional = node.style.resolvedMaxDimension(
      direction, dimension, ownerDim, ownerWidth);
    float maxInnerDim = maxDimensionOptional.isNull
      ? float.max : maxDimensionOptional - paddingAndBorder;

    availableInnerDim = availableInnerDim
      .minOrDefined(maxInnerDim)
      .maxOrDefined(minInnerDim);
  }

  return availableInnerDim;
}

private float computeFlexBasisForChildren(
  Node node,
  float availableInnerWidth,
  float availableInnerHeight,
  SizingMode widthSizingMode,
  SizingMode heightSizingMode,
  Direction direction,
  FlexDirection mainAxis,
  bool performLayout,
  ref LayoutData layoutMarkerData,
  uint depth,
  uint generationCount
) {
  float totalOuterFlexBasis = 0.0f;
  Node singleFlexChild = null;
  auto children = node.getLayoutChildren();
  SizingMode sizingModeMainDim = mainAxis.isRow
    ? widthSizingMode : heightSizingMode;
  
  if (sizingModeMainDim == SizingMode.StretchFit) {
    foreach (child; children) {
      if (child.isNodeFlexible) {
        if (
          singleFlexChild !is null ||
          child.resolveFlexGrow.inexactEquals(0.0f) ||
          child.resolveFlexShrink.inexactEquals(0.0f)
        ) {
          singleFlexChild = null;
        } else {
          singleFlexChild = child;
        }
      }
    }
  }

  foreach (child; children) {
    child.processDimensions();
    if (child.style.display == Display.None) {
      child.zeroOutLayoutRecursively();
      child.setHasNewLayout = true;
      child.setDirty = false;
      continue;
    }
    if (performLayout) {
      Direction childDirection = child.inheritDirection(direction);
      child.setPosition(
        childDirection,
        availableInnerWidth,
        availableInnerHeight
      );
    }

    if (child.style.positionType == PositionType.Absolute) {
      continue;
    }
    if (child == singleFlexChild) {
      child.setLayoutComputedFlexBasisGeneration = generationCount;
      child.setLayoutComputedFlexBasis = FloatOptional(0);
    } else {
      node.computeFlexBasisForChild(
        child,
        availableInnerWidth,
        widthSizingMode,
        availableInnerHeight,
        availableInnerWidth,
        availableInnerHeight,
        heightSizingMode,
        direction,
        layoutMarkerData,
        depth,
        generationCount
      );
    }

    totalOuterFlexBasis += child.getLayout.computedFlexBasis +
      child.style.computeMarginForAxis(mainAxis, availableInnerWidth);
  }

  return totalOuterFlexBasis;
}

private void computeFlexBasisForChild(
  Node node,
  Node child,
  float width,
  SizingMode widthMode,
  float height,
  float ownerWidth,
  float ownerHeight,
  SizingMode heightMode,
  Direction direction,
  ref LayoutData layoutMarkerData,
  uint depth,
  uint generationCount
) {
  FlexDirection mainAxis = node.style.flexDirection.resolveDirection(direction);
  bool isMainAxisRow = mainAxis.isRow;
  float mainAxisSize = isMainAxisRow ? width : height;
  float mainAxisOwnerSize = isMainAxisRow ? ownerWidth : ownerHeight;

  float childWidth = float.nan;
  float childHeight = float.nan;
  SizingMode childWidthSizingMode;
  SizingMode childHeightSizingMode;

  FloatOptional resolvedFlexBasis = child.resolveFlexBasis(
    direction, mainAxis, mainAxisOwnerSize, ownerWidth
  );
  bool isRowStyleDimDefined = child.hasDefiniteLength(
    Dimension.Width, ownerWidth
  );
  bool isColumnStyleDimDefined = child.hasDefiniteLength(
    Dimension.Height, ownerHeight
  );

  if (!resolvedFlexBasis.isNull && !mainAxisSize.isNaN) {
    if (child.getLayout.computedFlexBasis.isNull) {
      FloatOptional paddingAndBorder = FloatOptional(
        paddingAndBorderForAxis(child, mainAxis, direction, ownerWidth)
      );
      child.setLayoutComputedFlexBasis = resolvedFlexBasis
        .maxOrDefined(paddingAndBorder);
    }
  } else if (isMainAxisRow && isRowStyleDimDefined) {
    FloatOptional paddingAndBorder = FloatOptional(
      child.paddingAndBorderForAxis(
        FlexDirection.Row, direction, ownerWidth
      )
    );
    child.setLayoutComputedFlexBasis = child
      .getResolvedDimension(direction, Dimension.Width, ownerWidth, ownerWidth)
      .maxOrDefined(paddingAndBorder);
  } else if (!isMainAxisRow && isColumnStyleDimDefined) {
    FloatOptional paddingAndBorder = FloatOptional(
      child.paddingAndBorderForAxis(
        FlexDirection.Column, direction, ownerWidth
      )
    );
    child.setLayoutComputedFlexBasis = child
      .getResolvedDimension(direction, Dimension.Height, ownerHeight, ownerWidth)
      .maxOrDefined(paddingAndBorder);
  } else {
    childWidthSizingMode = SizingMode.MaxContent;
    childHeightSizingMode = SizingMode.MaxContent;

    auto marginRow = child.style.computeMarginForAxis(
      FlexDirection.Row, ownerWidth);
    auto marginColumn = child.style.computeMarginForAxis(
      FlexDirection.Column, ownerWidth);

    if (isRowStyleDimDefined) {
      childWidth = child.getResolvedDimension(
        direction, Dimension.Width, ownerWidth, ownerWidth) +
        marginRow;
      childWidthSizingMode = SizingMode.StretchFit;
    }
    if (isColumnStyleDimDefined) {
      childHeight = child.getResolvedDimension(
        direction, Dimension.Height, ownerHeight, ownerWidth) +
        marginColumn;
      childHeightSizingMode = SizingMode.StretchFit;
    }

    if (
      (!isMainAxisRow && node.style.overflow == Overflow.Scroll) ||
      node.style.overflow != Overflow.Scroll
    ) {
      if (childWidth.isNaN && !width.isNaN) {
        childWidth = width;
        childWidthSizingMode = SizingMode.FitContent;
      }
    }

    if (
      (isMainAxisRow && node.style.overflow == Overflow.Scroll) ||
      node.style.overflow != Overflow.Scroll 
    ) {
      if (childHeight.isNaN && !height.isNaN) {
        childHeight = height;
        childHeightSizingMode = SizingMode.FitContent;
      }
    }

    auto ref childStyle = child.style;
    if (!childStyle.aspectRatio.isNull) {
      if (!isMainAxisRow && childWidthSizingMode == SizingMode.StretchFit) {
        childHeight = marginColumn +
          (childWidth - marginRow) /
          childStyle.aspectRatio;
        childHeightSizingMode = SizingMode.StretchFit;
      } else if (
        isMainAxisRow && childHeightSizingMode == SizingMode.StretchFit) {
        childWidth = marginRow +
          (childHeight - marginColumn) *
          childStyle.aspectRatio;
        childWidthSizingMode = SizingMode.StretchFit;
      }
    }

    bool hasExactWidth = !width.isNaN && widthMode == SizingMode.StretchFit;
    bool childWidthStretch = node.resolveChildAlignment(child) ==
      Align.Stretch && childWidthSizingMode != SizingMode.StretchFit;
    if (!isMainAxisRow && !isRowStyleDimDefined &&
      hasExactWidth && childWidthStretch) {
      childWidth = width;
      childWidthSizingMode = SizingMode.StretchFit;

      if (!childStyle.aspectRatio.isNull) {
        childHeight = (childWidth - marginRow) /
          childStyle.aspectRatio;
        childHeightSizingMode = SizingMode.StretchFit;
      }
    }

    bool hasExactHeight = !height.isNaN && heightMode == SizingMode.StretchFit;
    bool childHeightStretch = node.resolveChildAlignment(child) ==
      Align.Stretch && childHeightSizingMode != SizingMode.StretchFit;
    if (isMainAxisRow && !isColumnStyleDimDefined &&
      hasExactHeight && childHeightStretch) {
      childHeight = height;
      childHeightSizingMode = SizingMode.StretchFit;

      if (!childStyle.aspectRatio.isNull) {
        childWidth = (childHeight - marginColumn) *
          childStyle.aspectRatio;
        childWidthSizingMode = SizingMode.StretchFit;
      }
    }

    child.constrainMaxSizeForMode(
      direction,
      FlexDirection.Row,
      ownerWidth,
      ownerWidth,
      childWidthSizingMode,
      childWidth
    );
    child.constrainMaxSizeForMode(
      direction,
      FlexDirection.Column,
      ownerHeight,
      ownerWidth,
      childHeightSizingMode,
      childHeight
    );

    child.calculateLayoutInternal(
      childWidth,
      childHeight,
      direction,
      childWidthSizingMode,
      childHeightSizingMode,
      ownerWidth,
      ownerHeight,
      false,
      LayoutPassReason.kMeasureChild,
      layoutMarkerData,
      depth,
      generationCount
    );

    child.setLayoutComputedFlexBasis = FloatOptional(
      child.getLayout.measuredDimension(mainAxis.dimension)
        .maxOrDefined = child.paddingAndBorderForAxis(
          mainAxis, direction, ownerWidth)
    );
  }
  child.setLayoutComputedFlexBasisGeneration = generationCount;
}

private void constrainMaxSizeForMode(
  Node node,
  Direction direction,
  FlexDirection axis,
  float ownerAxisSize,
  float ownerWidth,
  ref SizingMode mode,
  ref float size
) {
  FloatOptional maxSize = node.style.resolvedMaxDimension(
    direction, axis.dimension, ownerAxisSize, ownerWidth) +
  FloatOptional(node.style.computeMarginForAxis(axis, ownerWidth));
  final switch (mode) {
    case SizingMode.StretchFit:
    case SizingMode.FitContent:
      size = (maxSize.isNull || size < maxSize)
        ? size : maxSize;
      break;
    case SizingMode.MaxContent:
      if (!maxSize.isNull) {
        mode = SizingMode.FitContent;
        size = maxSize;
      }
      break;
  }
}

private void zeroOutLayoutRecursively(Node node) {
  node.getLayout = LayoutResults();
  node.setLayoutDimension(0, Dimension.Width);
  node.setLayoutDimension(0, Dimension.Height);
  node.setHasNewLayout = true;

  node.cloneChildrenIfNeeded();
  foreach (child; node.getChildren) {
    child.zeroOutLayoutRecursively();
  }
}
