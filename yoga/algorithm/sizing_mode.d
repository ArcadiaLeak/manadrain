module yoga.algorithm.sizing_mode;

import yoga;

enum SizingMode {
  StretchFit,
  MaxContent,
  FitContent
}

MeasureMode measureMode(SizingMode mode) {
  final switch (mode) {
    case SizingMode.StretchFit:
      return MeasureMode.Exactly;
    case SizingMode.MaxContent:
      return MeasureMode.Undefined;
    case SizingMode.FitContent:
      return MeasureMode.AtMost;
  }
}

SizingMode sizingMode(MeasureMode mode) {
  final switch (mode) {
    case MeasureMode.Exactly:
      return SizingMode.StretchFit;
    case MeasureMode.Undefined:
      return SizingMode.MaxContent;
    case MeasureMode.AtMost:
      return SizingMode.FitContent;
  }
}