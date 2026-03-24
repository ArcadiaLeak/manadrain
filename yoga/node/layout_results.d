module yoga.node.layout_results;

import std.math;
import yoga;

struct LayoutResults {
  static const int MaxCachedMeasurements = 8;

  uint computedFlexBasisGeneration;
  FloatOptional computedFlexBasis;

  uint generationCount;
  Direction lastOwnerDirection = Direction.Inherit;

  uint nextCachedMeasurementsIndex;
  CachedMeasurement[MaxCachedMeasurements] cachedMeasurements;

  CachedMeasurement cachedLayout;

    Direction direction() pure inout {
    return direction_;
  }

  void setDirection(Direction direction) {
    direction_ = direction;
  }

  bool hadOverflow() pure inout {
    return hadOverflow_;
  }

  void setHadOverflow(bool hadOverflow) {
    hadOverflow_ = hadOverflow;
  }

  float dimension(Dimension axis) pure inout {
    return dimensions_[axis];
  }

  void setDimension(Dimension axis, float dimension) {
    dimensions_[axis] = dimension;
  }

  float measuredDimension(Dimension axis) pure inout {
    return measuredDimensions_[axis];
  }

  float rawDimension(Dimension axis) pure inout {
    return rawDimensions_[axis];
  }

  void setMeasuredDimension(Dimension axis, float dimension) {
    measuredDimensions_[axis] = dimension;
  }

  void setRawDimension(Dimension axis, float dimension) {
    rawDimensions_[axis] = dimension;
  }

  float position(PhysicalEdge physicalEdge) pure inout {
    return position_[physicalEdge];
  }

  void setPosition(PhysicalEdge physicalEdge, float dimension) {
    position_[physicalEdge] = dimension;
  }

  float margin(PhysicalEdge physicalEdge) pure inout {
    return margin_[physicalEdge];
  }

  void setMargin(PhysicalEdge physicalEdge, float dimension) {
    margin_[physicalEdge] = dimension;
  }

  float border(PhysicalEdge physicalEdge) pure inout {
    return border_[physicalEdge];
  }

  void setBorder(PhysicalEdge physicalEdge, float dimension) {
    border_[physicalEdge] = dimension;
  }

  float padding(PhysicalEdge physicalEdge) pure inout {
    return padding_[physicalEdge];
  }

  void setPadding(PhysicalEdge physicalEdge, float dimension) {
    padding_[physicalEdge] = dimension;
  }

  bool opEquals(LayoutResults layout) pure {
    bool isEqual = inexactEquals(position_, layout.position_) &&
      inexactEquals(dimensions_, layout.dimensions_) &&
      inexactEquals(margin_, layout.margin_) &&
      inexactEquals(border_, layout.border_) &&
      inexactEquals(padding_, layout.padding_) &&
      direction() == layout.direction() &&
      hadOverflow() == layout.hadOverflow() &&
      lastOwnerDirection == layout.lastOwnerDirection &&
      nextCachedMeasurementsIndex == layout.nextCachedMeasurementsIndex &&
      cachedLayout == layout.cachedLayout &&
      computedFlexBasis == layout.computedFlexBasis;

    for (
      uint i = 0;
      i < LayoutResults.MaxCachedMeasurements && isEqual;
      ++i
    ) {
      isEqual = isEqual && cachedMeasurements[i] == layout.cachedMeasurements[i];
    }

    if (
      !isNaN(measuredDimensions_[0]) ||
      !isNaN(layout.measuredDimensions_[0])
    ) {
      isEqual =
        isEqual && (measuredDimensions_[0] == layout.measuredDimensions_[0]);
    }
    if (
      !isNaN(measuredDimensions_[1]) ||
      !isNaN(layout.measuredDimensions_[1])
    ) {
      isEqual =
        isEqual && (measuredDimensions_[1] == layout.measuredDimensions_[1]);
  }

    return isEqual;
  }
private:
  Direction direction_ = Direction.Inherit;
  bool hadOverflow_ = false;

  float[2] dimensions_ = [float.nan, float.nan];
  float[2] measuredDimensions_ = [float.nan, float.nan];
  float[2] rawDimensions_ = [float.nan, float.nan];
  float[4] position_;
  float[4] margin_;
  float[4] border_;
  float[4] padding_;
}
