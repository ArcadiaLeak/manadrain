module yoga.style;

import std.algorithm.comparison;
import std.traits;

import yoga;

public import yoga.style.small_value_buffer;
public import yoga.style.style_length;
public import yoga.style.style_size_length;
public import yoga.style.style_value_handle;
public import yoga.style.style_value_pool;

struct Style {
  enum float DefaultFlexGrow = 0.0f;
  enum float DefaultFlexShrink = 0.0f;

  Direction direction() pure inout {
    return direction_;
  }
  void setDirection(Direction value) {
    direction_ = value;
  }

  FlexDirection flexDirection() pure inout {
    return flexDirection_;
  }
  void setFlexDirection(FlexDirection value) {
    flexDirection_ = value;
  }

  Justify justifyContent() pure inout {
    return justifyContent_;
  }
  void setJustifyContent(Justify value) {
    justifyContent_ = value;
  }

  StyleSizeLength flexBasis() pure inout {
    return pool_.getSize(flexBasis_);
  }
  void setFlexBasis(StyleSizeLength value) {
    pool_.store(flexBasis_, value);
  }

  StyleLength margin(Edge edge) pure inout {
    return pool_.getLength(margin_[edge]);
  }
  void setMargin(Edge edge, StyleLength value) {
    pool_.store(margin_[edge], value);
  }

  StyleLength position(Edge edge) pure inout {
    return pool_.getLength(position_[edge]);
  }
  void setPosition(Edge edge, StyleLength value) {
    pool_.store(position_[edge], value);
  }

  StyleLength padding(Edge edge) pure inout {
    return pool_.getLength(padding_[edge]);
  }
  void setPadding(Edge edge, StyleLength value) {
    pool_.store(padding_[edge], value);
  }

  StyleLength border(Edge edge) pure inout {
    return pool_.getLength(border_[edge]);
  }
  void setBorder(Edge edge, StyleLength value) {
    pool_.store(border_[edge], value);
  }

  StyleLength gap(Gutter gutter) pure inout {
    return pool_.getLength(gap_[gutter]);
  }
  void setGap(Gutter gutter, StyleLength value) {
    pool_.store(gap_[gutter], value);
  }

  StyleSizeLength dimension(Dimension axis) pure inout {
    return pool_.getSize(dimensions_[axis]);
  }
  void setDimension(Dimension axis, StyleSizeLength value) {
    pool_.store(dimensions_[axis], value);
  }

  StyleSizeLength minDimension(Dimension axis) pure inout {
    return pool_.getSize(minDimensions_[axis]);
  }
  void setMinDimension(Dimension axis, StyleSizeLength value) {
    pool_.store(minDimensions_[axis], value);
  }

  Align alignContent() pure inout {
    return alignContent_;
  }
  void setAlignContent(Align value) {
    alignContent_ = value;
  }

  Align alignItems() pure inout {
    return alignItems_;
  }
  void setAlignItems(Align value) {
    alignItems_ = value;
  }

  Align alignSelf() pure inout {
    return alignSelf_;
  }
  void setAlignSelf(Align value) {
    alignSelf_ = value;
  }

  PositionType positionType() pure inout {
    return positionType_;
  }
  void setPositionType(PositionType value) {
    positionType_ = value;
  }

  Wrap flexWrap() pure inout {
    return flexWrap_;
  }
  void setFlexWrap(Wrap value) {
    flexWrap_ = value;
  }

  Overflow overflow() pure inout {
    return overflow_;
  }
  void setOverflow(Overflow value) {
    overflow_ = value;
  }

  Display display() pure inout {
    return display_;
  }
  void setDisplay(Display value) {
    display_ = value;
  }

  FloatOptional flex() pure inout {
    return pool_.getNumber(flex_);
  }
  void setFlex(FloatOptional value) {
    pool_.store(flex_, value);
  }

  FloatOptional flexGrow() pure inout {
    return pool_.getNumber(flexGrow_);
  }
  void setFlexGrow(FloatOptional value) {
    pool_.store(flexGrow_, value);
  }

  FloatOptional flexShrink() pure inout {
    return pool_.getNumber(flexShrink_);
  }
  void setFlexShrink(FloatOptional value) {
    pool_.store(flexShrink_, value);
  }

  BoxSizing boxSizing() pure inout {
    return boxSizing_;
  }
  void setBoxSizing(BoxSizing value) {
    boxSizing_ = value;
  }

  StyleLength computeLeftEdge(
    Edges edges,
    Direction layoutDirection
  ) pure inout {
    if (
      layoutDirection == Direction.LTR &&
      edges[Edge.Start].isDefined()
    ) {
      return pool_.getLength(edges[Edge.Start]);
    } else if (
      layoutDirection == Direction.RTL &&
      edges[Edge.End].isDefined()
    ) {
      return pool_.getLength(edges[Edge.End]);
    } else if (edges[Edge.Left].isDefined()) {
      return pool_.getLength(edges[Edge.Left]);
    } else if (edges[Edge.Horizontal].isDefined()) {
      return pool_.getLength(edges[Edge.Horizontal]);
    } else {
      return pool_.getLength(edges[Edge.All]);
    }
  }

  StyleLength computeTopEdge(Edges edges) pure inout {
    if (edges[Edge.Top].isDefined()) {
      return pool_.getLength(edges[Edge.Top]);
    } else if (edges[Edge.Vertical].isDefined()) {
      return pool_.getLength(edges[Edge.Vertical]);
    } else {
      return pool_.getLength(edges[Edge.All]);
    }
  }

  StyleLength computeRightEdge(
    Edges edges,
    Direction layoutDirection
  ) pure inout {
    if (
      layoutDirection == Direction.LTR &&
      edges[Edge.End].isDefined()
    ) {
      return pool_.getLength(edges[Edge.End]);
    } else if (
      layoutDirection == Direction.RTL &&
      edges[Edge.Start].isDefined()
    ) {
      return pool_.getLength(edges[Edge.Start]);
    } else if (edges[Edge.Right].isDefined()) {
      return pool_.getLength(edges[Edge.Right]);
    } else if (edges[Edge.Horizontal].isDefined()) {
      return pool_.getLength(edges[Edge.Horizontal]);
    } else {
      return pool_.getLength(edges[Edge.All]);
    }
  }

  StyleLength computeBottomEdge(Edges edges) pure inout {
    if (edges[Edge.Bottom].isDefined()) {
      return pool_.getLength(edges[Edge.Bottom]);
    } else if (edges[Edge.Vertical].isDefined()) {
      return pool_.getLength(edges[Edge.Vertical]);
    } else {
      return pool_.getLength(edges[Edge.All]);
    }
  }

  float computeFlexStartPadding(
    FlexDirection axis,
    Direction direction,
    float widthSize
  ) pure inout {
    return maxOrDefined(
      computePadding(flexStartEdge(axis), direction)
        .resolve(widthSize),
      0.0f
    );
  }

  float computeInlineStartPadding(
    FlexDirection axis,
    Direction direction,
    float widthSize
  ) pure inout {
    return maxOrDefined(
      computePadding(inlineStartEdge(axis, direction), direction)
        .resolve(widthSize),
      0.0f
    );
  }

  float computeFlexEndPadding(
    FlexDirection axis,
    Direction direction,
    float widthSize
  ) pure inout {
    return maxOrDefined(
      computePadding(flexEndEdge(axis), direction)
        .resolve(widthSize),
      0.0f
    );
  }

  float computeInlineEndPadding(
    FlexDirection axis,
    Direction direction,
    float widthSize
  ) pure inout {
    return maxOrDefined(
      computePadding(inlineEndEdge(axis, direction), direction)
        .resolve(widthSize),
      0.0f
    );
  }

  float computeFlexStartBorder(
    FlexDirection axis,
    Direction direction
  ) pure inout {
    return maxOrDefined(
      computeBorder(flexStartEdge(axis), direction).resolve(0.0f),
      0.0f
    );
  }

  float computeFlexEndBorder(
    FlexDirection axis,
    Direction direction
  ) pure inout {
    return maxOrDefined(
      computeBorder(flexEndEdge(axis), direction).resolve(0.0f),
      0.0f
    );
  }

  float computeFlexEndBorder(
    FlexDirection axis,
    Direction direction,
    float widthSize
  ) pure inout {
    return maxOrDefined(
      computeBorder(flexEndEdge(axis), direction)
        .resolve(widthSize),
      0.0f
    );
  }

  float computeFlexStartPaddingAndBorder(
    FlexDirection axis,
    Direction direction,
    float widthSize
  ) pure inout {
    return computeFlexStartPadding(axis, direction, widthSize) +
      computeFlexStartBorder(axis, direction);
  }

  float computeFlexEndPaddingAndBorder(
    FlexDirection axis,
    Direction direction,
    float widthSize
  ) pure inout {
    return computeFlexEndPadding(axis, direction, widthSize) +
      computeFlexEndBorder(axis, direction);
  }

  float computePaddingAndBorderForDimension(
    Direction direction,
    Dimension dimension,
    float widthSize
  ) pure inout {
    FlexDirection flexDirectionForDimension = dimension == Dimension.Width
      ? FlexDirection.Row
      : FlexDirection.Column;

    return computeFlexStartPaddingAndBorder(
      flexDirectionForDimension,
      direction,
      widthSize
    ) + computeFlexEndPaddingAndBorder(
      flexDirectionForDimension,
      direction,
      widthSize
    );
  }

  FloatOptional resolvedMinDimension(
    Direction direction,
    Dimension axis,
    float referenceLength,
    float ownerWidth
  ) pure {
    FloatOptional value = minDimension(axis).resolve(referenceLength);
    if (boxSizing() == BoxSizing.BorderBox) {
      return value;
    }

    FloatOptional dimensionPaddingAndBorder = FloatOptional(
      computePaddingAndBorderForDimension(direction, axis, ownerWidth)
    );

    return FloatOptional(value + dimensionPaddingAndBorder.isNull
      ? FloatOptional(0.0) : dimensionPaddingAndBorder);
  }

  StyleSizeLength maxDimension(Dimension axis) pure inout {
    return pool_.getSize(maxDimensions_[axis]);
  }
  void setMaxDimension(Dimension axis, StyleSizeLength value) {
    pool_.store(maxDimensions_[axis], value);
  }

  FloatOptional resolvedMaxDimension(
    Direction direction,
    Dimension axis,
    float referenceLength,
    float ownerWidth
  ) pure {
    FloatOptional value = maxDimension(axis).resolve(referenceLength);
    if (boxSizing() == BoxSizing.BorderBox) {
      return value;
    }

    FloatOptional dimensionPaddingAndBorder = FloatOptional(
      computePaddingAndBorderForDimension(direction, axis, ownerWidth)
    );

    return FloatOptional(value + dimensionPaddingAndBorder.isNull
      ? FloatOptional(0.0) : dimensionPaddingAndBorder);
  }

  float computeInlineStartMargin(
    FlexDirection axis,
    Direction direction,
    float widthSize
  ) pure inout {
    auto result = computeMargin(axis.inlineStartEdge(direction), direction)
      .resolve(widthSize);

    return result.isNull ? 0.0f : result;
  }

  float computeInlineEndMargin(
    FlexDirection axis,
    Direction direction,
    float widthSize
  ) pure inout {
    auto result = computeMargin(axis.inlineEndEdge(direction), direction)
      .resolve(widthSize);

    if (result.isNull) {
      return 0.0f;
    }

    return result.isNull ? 0.0f : result;
  }

  float computeMarginForAxis(FlexDirection axis, float widthSize) pure inout {
    return computeInlineStartMargin(axis, Direction.LTR, widthSize) +
      computeInlineEndMargin(axis, Direction.LTR, widthSize);
  }

  bool opEquals(in Style other) pure {
    return direction_ == other.direction_ &&
      flexDirection_ == other.flexDirection_ &&
      justifyContent_ == other.justifyContent_ &&
      alignContent_ == other.alignContent_ &&
      alignItems_ == other.alignItems_ && alignSelf_ == other.alignSelf_ &&
      positionType_ == other.positionType_ && flexWrap_ == other.flexWrap_ &&
      overflow_ == other.overflow_ && display_ == other.display_ &&
      numbersEqual(flex_, pool_, other.flex_, other.pool_) &&
      numbersEqual(flexGrow_, pool_, other.flexGrow_, other.pool_) &&
      numbersEqual(flexShrink_, pool_, other.flexShrink_, other.pool_) &&
      lengthsEqual(flexBasis_, pool_, other.flexBasis_, other.pool_) &&
      lengthsEqual(margin_, pool_, other.margin_, other.pool_) &&
      lengthsEqual(position_, pool_, other.position_, other.pool_) &&
      lengthsEqual(padding_, pool_, other.padding_, other.pool_) &&
      lengthsEqual(border_, pool_, other.border_, other.pool_) &&
      lengthsEqual(gap_, pool_, other.gap_, other.pool_) &&
      lengthsEqual(dimensions_, pool_, other.dimensions_, other.pool_) &&
      lengthsEqual(
        minDimensions_,
        pool_,
        other.minDimensions_,
        other.pool_
      ) &&
      lengthsEqual(
        maxDimensions_,
        pool_,
        other.maxDimensions_,
        other.pool_
      ) &&
      numbersEqual(
        aspectRatio_,
        pool_,
        other.aspectRatio_,
        other.pool_
      );
  }

  this(ref inout(Style) src) pure {
    opAssign(src);
  }

  ref Style opAssign(ref inout(Style) src) pure {
    foreach (i, ref inout field; src.tupleof)
      this.tupleof[i] = field;
    return this;
  }

  bool isInlineStartPositionDefined(FlexDirection axis, Direction direction)
    pure inout {
    return computePosition(inlineStartEdge(axis, direction), direction)
      .isDefined();
  }

  bool isInlineStartPositionAuto(FlexDirection axis, Direction direction)
    pure inout {
    return computePosition(inlineStartEdge(axis, direction), direction)
      .isAuto();
  }

  float computeInlineStartPosition(
    FlexDirection axis,
    Direction direction,
    float axisSize
  ) pure inout {
    auto result = computePosition(inlineStartEdge(axis, direction), direction)
      .resolve(axisSize);

    return result.isNull ? 0.0f : result.get;
  }

  float computeInlineEndPosition(
    FlexDirection axis,
    Direction direction,
    float axisSize
  ) pure inout {
    auto result = computePosition(inlineEndEdge(axis, direction), direction)
      .resolve(axisSize);

    return result.isNull ? 0.0f : result.get;
  }

  float computeInlineStartBorder(
    FlexDirection axis,
    Direction direction
  ) pure inout {
    return maxOrDefined(
      computeBorder(inlineStartEdge(axis, direction), direction)
        .resolve(0.0f),
      0.0f
    );
  }

  float computeInlineEndBorder(
    FlexDirection axis,
    Direction direction
  ) pure inout {
    return maxOrDefined(
      computeBorder(inlineEndEdge(axis, direction), direction)
        .resolve(0.0f),
      0.0f
    );
  }

  float computeInlineStartPaddingAndBorder(
    FlexDirection axis,
    Direction direction,
    float widthSize
  ) pure inout {
    return computeInlineStartPadding(axis, direction, widthSize) +
      computeInlineStartBorder(axis, direction);
  }

  float computeInlineEndPaddingAndBorder(
    FlexDirection axis,
    Direction direction,
    float widthSize
  ) pure inout {
    return computeInlineEndPadding(axis, direction, widthSize) +
      computeInlineEndBorder(axis, direction);
  }

  FloatOptional aspectRatio() pure inout {
    return pool_.getNumber(aspectRatio_);
  }

  float computeGapForAxis(FlexDirection axis, float ownerSize) pure inout {
    auto gap = axis.isRow ? computeColumnGap() : computeRowGap();
    return gap.resolve(ownerSize).maxOrDefined(0.0f);
  }

  bool flexStartMarginIsAuto(FlexDirection axis, Direction direction) pure inout {
    return computeMargin(flexStartEdge(axis), direction).isAuto();
  }

  bool flexEndMarginIsAuto(FlexDirection axis, Direction direction) pure inout {
    return computeMargin(flexEndEdge(axis), direction).isAuto();
  }

private:
  static bool numbersEqual(
    const StyleValueHandle lhsHandle,
    in StyleValuePool lhsPool,
    const StyleValueHandle rhsHandle,
    in StyleValuePool rhsPool
  ) pure {
    return (lhsHandle.isUndefined() && rhsHandle.isUndefined()) ||
      (lhsPool.getNumber(lhsHandle) == rhsPool.getNumber(rhsHandle));
  }

  static bool lengthsEqual(
    const StyleValueHandle lhsHandle,
    in StyleValuePool lhsPool,
    const StyleValueHandle rhsHandle,
    in StyleValuePool rhsPool
  ) pure {
    return (lhsHandle.isUndefined() && rhsHandle.isUndefined()) ||
      (lhsPool.getLength(lhsHandle) == rhsPool.getLength(rhsHandle));
  }

  static bool lengthsEqual(
    const StyleValueHandle[] lhs,
    in StyleValuePool lhsPool,
    const StyleValueHandle[] rhs,
    in StyleValuePool rhsPool
  ) pure {
    return equal!(
      (lft, rgt) => lengthsEqual(lft, lhsPool, rgt, rhsPool)
    )(lhs, rhs);
  }

  StyleLength computeColumnGap() pure inout {
    if (gap_[Gutter.Column].isDefined()) {
      return pool_.getLength(gap_[Gutter.Column]);
    } else {
      return pool_.getLength(gap_[Gutter.All]);
    }
  }

  StyleLength computeRowGap() pure inout {
    if (gap_[Gutter.Row].isDefined()) {
      return pool_.getLength(gap_[Gutter.Row]);
    } else {
      return pool_.getLength(gap_[Gutter.All]);
    }
  }

  StyleLength computeLeftEdge(Edges edges, Direction layoutDirection) pure {
    if (
      layoutDirection == Direction.LTR &&
      edges[Edge.Start].isDefined()
    ) {
      return pool_.getLength(edges[Edge.Start]);
    } else if (
      layoutDirection == Direction.RTL &&
      edges[Edge.End].isDefined()
    ) {
      return pool_.getLength(edges[Edge.End]);
    } else if (edges[Edge.Left].isDefined()) {
      return pool_.getLength(edges[Edge.Left]);
    } else if (edges[Edge.Horizontal].isDefined()) {
      return pool_.getLength(edges[Edge.Horizontal]);
    } else {
      return pool_.getLength(edges[Edge.All]);
    }
  }

  StyleLength computeTopEdge(Edges edges) pure {
    if (edges[Edge.Top].isDefined()) {
      return pool_.getLength(edges[Edge.Top]);
    } else if (edges[Edge.Vertical].isDefined()) {
      return pool_.getLength(edges[Edge.Vertical]);
    } else {
      return pool_.getLength(edges[Edge.All]);
    }
  }

  StyleLength computeRightEdge(Edges edges, Direction layoutDirection) pure {
    if (
      layoutDirection == Direction.LTR &&
      edges[Edge.End].isDefined
    ) {
      return pool_.getLength(edges[Edge.End]);
    } else if (
      layoutDirection == Direction.RTL &&
      edges[Edge.Start].isDefined
    ) {
      return pool_.getLength(edges[Edge.Start]);
    } else if (edges[Edge.Right].isDefined) {
      return pool_.getLength(edges[Edge.Right]);
    } else if (edges[Edge.Horizontal].isDefined) {
      return pool_.getLength(edges[Edge.Horizontal]);
    } else {
      return pool_.getLength(edges[Edge.All]);
    }
  }

  StyleLength computeBottomEdge(Edges edges) pure {
    if (edges[Edge.Bottom].isDefined()) {
      return pool_.getLength(edges[Edge.Bottom]);
    } else if (edges[Edge.Vertical].isDefined()) {
      return pool_.getLength(edges[Edge.Vertical]);
    } else {
      return pool_.getLength(edges[Edge.All]);
    }
  }

  StyleLength computeMargin(
    PhysicalEdge edge,
    Direction direction
  ) pure inout {
    final switch (edge) {
      case PhysicalEdge.Left:
        return computeLeftEdge(margin_, direction);
      case PhysicalEdge.Top:
        return computeTopEdge(margin_);
      case PhysicalEdge.Right:
        return computeRightEdge(margin_, direction);
      case PhysicalEdge.Bottom:
        return computeBottomEdge(margin_);
    }
  }

  StyleLength computePadding(
    PhysicalEdge edge,
    Direction direction
  ) pure inout {
    final switch (edge) {
      case PhysicalEdge.Left:
        return computeLeftEdge(padding_, direction);
      case PhysicalEdge.Top:
        return computeTopEdge(padding_);
      case PhysicalEdge.Right:
        return computeRightEdge(padding_, direction);
      case PhysicalEdge.Bottom:
        return computeBottomEdge(padding_);
    }
  }

  StyleLength computeBorder(
    PhysicalEdge edge,
    Direction direction
  ) pure inout {
    final switch (edge) {
      case PhysicalEdge.Left:
        return computeLeftEdge(border_, direction);
      case PhysicalEdge.Top:
        return computeTopEdge(border_);
      case PhysicalEdge.Right:
        return computeRightEdge(border_, direction);
      case PhysicalEdge.Bottom:
        return computeBottomEdge(border_);
    }
  }

  StyleLength computePosition(
    PhysicalEdge edge,
    Direction direction
  ) pure inout {
    final switch (edge) {
      case PhysicalEdge.Left:
        return computeLeftEdge(position_, direction);
      case PhysicalEdge.Top:
        return computeTopEdge(position_);
      case PhysicalEdge.Right:
        return computeRightEdge(position_, direction);
      case PhysicalEdge.Bottom:
        return computeBottomEdge(position_);
    }
  }

  alias Dimensions = StyleValueHandle[EnumMembers!Dimension.length];
  alias Edges = StyleValueHandle[EnumMembers!Edge.length];
  alias Gutters = StyleValueHandle[EnumMembers!Gutter.length];

  Direction direction_ = Direction.Inherit;
  FlexDirection flexDirection_ = FlexDirection.Column;
  Justify justifyContent_ = Justify.FlexStart;
  Align alignContent_ = Align.FlexStart;
  Align alignItems_ = Align.Stretch;
  Align alignSelf_ = Align.Auto;
  PositionType positionType_ = PositionType.Relative;
  Wrap flexWrap_ = Wrap.NoWrap;
  Overflow overflow_ = Overflow.Visible;
  Display display_ = Display.Flex;
  BoxSizing boxSizing_ = BoxSizing.BorderBox;

  StyleValueHandle flex_;
  StyleValueHandle flexGrow_;
  StyleValueHandle flexShrink_;
  StyleValueHandle flexBasis_ = StyleValueHandle.ofAuto;
  Edges margin_;
  Edges position_;
  Edges padding_;
  Edges border_;
  Gutters gap_;
  Dimensions dimensions_ = [
    StyleValueHandle.ofAuto,
    StyleValueHandle.ofAuto
  ];
  Dimensions minDimensions_;
  Dimensions maxDimensions_;
  StyleValueHandle aspectRatio_;

  StyleValuePool pool_;
}
