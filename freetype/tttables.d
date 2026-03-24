module freetype.tttables;

import freetype;

struct TT_PCLT {
  FT_Fixed Version;
  FT_ULong FontNumber;
  FT_UShort Pitch;
  FT_UShort xHeight;
  FT_UShort Style;
  FT_UShort TypeFamily;
  FT_UShort CapHeight;
  FT_UShort SymbolSet;
  FT_Char[16] TypeFace;
  FT_Char[8] CharacterComponent;
  FT_Char[6] FileName;
  FT_Char StrokeWeight;
  FT_Char WidthType;
  FT_Byte SerifStyle;
  FT_Byte Reserved;
}

struct TT_Postscript {
  FT_Fixed FormatType;
  FT_Fixed italicAngle;
  FT_Short underlinePosition;
  FT_Short underlineThickness;
  FT_ULong isFixedPitch;
  FT_ULong minMemType42;
  FT_ULong maxMemType42;
  FT_ULong minMemType1;
  FT_ULong maxMemType1;
}

struct TT_OS2 {
  FT_UShort version_;
  FT_Short xAvgCharWidth;
  FT_UShort usWeightClass;
  FT_UShort usWidthClass;
  FT_UShort fsType;
  FT_Short ySubscriptXSize;
  FT_Short ySubscriptYSize;
  FT_Short ySubscriptXOffset;
  FT_Short ySubscriptYOffset;
  FT_Short ySuperscriptXSize;
  FT_Short ySuperscriptYSize;
  FT_Short ySuperscriptXOffset;
  FT_Short ySuperscriptYOffset;
  FT_Short yStrikeoutSize;
  FT_Short yStrikeoutPosition;
  FT_Short sFamilyClass;

  FT_Byte[10] panose;

  FT_ULong ulUnicodeRange1;
  FT_ULong ulUnicodeRange2;
  FT_ULong ulUnicodeRange3;
  FT_ULong ulUnicodeRange4;

  FT_Char[4] achVendID;

  FT_UShort fsSelection;
  FT_UShort usFirstCharIndex;
  FT_UShort usLastCharIndex;
  FT_Short sTypoAscender;
  FT_Short sTypoDescender;
  FT_Short sTypoLineGap;
  FT_UShort usWinAscent;
  FT_UShort usWinDescent;

  FT_ULong ulCodePageRange1;
  FT_ULong ulCodePageRange2;

  FT_Short sxHeight;
  FT_Short sCapHeight;
  FT_UShort usDefaultChar;
  FT_UShort usBreakChar;
  FT_UShort usMaxContent;

  FT_UShort usLowerOpticalPointSize;
  FT_UShort usUpperOpticalPointSize;
}

struct TT_VertHeader {
  FT_Fixed Version;
  FT_Short Ascender;
  FT_Short Descender;
  FT_Short Line_Gap;

  FT_UShort advance_Height_Max;

  FT_Short min_Top_Side_Bearing;
  FT_Short min_Bottom_Side_Bearing;
  FT_Short yMax_Extent;
  FT_Short caret_Slope_Rise;
  FT_Short caret_Slope_Run;
  FT_Short caret_Offset;

  FT_Short[4] Reserved;

  FT_Short metric_Data_Format;
  FT_UShort number_Of_VMetrics;

  void* long_metrics;
  void* short_metrics;
}

struct TT_MaxProfile {
  FT_Fixed version_;
  FT_UShort numGlyphs;
  FT_UShort maxPoints;
  FT_UShort maxContours;
  FT_UShort maxCompositePoints;
  FT_UShort maxCompositeContours;
  FT_UShort maxZones;
  FT_UShort maxTwilightPoints;
  FT_UShort maxStorage;
  FT_UShort maxFunctionDefs;
  FT_UShort maxInstructionDefs;
  FT_UShort maxStackElements;
  FT_UShort maxSizeOfInstructions;
  FT_UShort maxComponentElements;
  FT_UShort maxComponentDepth;
}

struct TT_HoriHeader {
  FT_Fixed Version;
  FT_Short Ascender;
  FT_Short Descender;
  FT_Short Line_Gap;

  FT_UShort advance_Width_Max;

  FT_Short min_Left_Side_Bearing;
  FT_Short min_Right_Side_Bearing;
  FT_Short xMax_Extent;
  FT_Short caret_Slope_Rise;
  FT_Short caret_Slope_Run;
  FT_Short caret_Offset;

  FT_Short[4] Reserved;

  FT_Short metric_Data_Format;
  FT_UShort number_Of_HMetrics;

  void* long_metrics;
  void* short_metrics;
}

struct TT_Header {
  FT_Fixed Table_Version;
  FT_Fixed Font_Revision;

  FT_Long CheckSum_Adjust;
  FT_Long Magic_Number;

  FT_UShort Flags;
  FT_UShort Units_Per_EM;

  FT_ULong[2] Created;
  FT_ULong[2] Modified;

  FT_Short xMin;
  FT_Short yMin;
  FT_Short xMax;
  FT_Short yMax;

  FT_UShort Mac_Style;
  FT_UShort Lowest_Rec_PPEM;

  FT_Short Font_Direction;
  FT_Short Index_To_Loc_Format;
  FT_Short Glyph_Data_Format;
}
