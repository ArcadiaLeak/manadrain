module freetype.truetype.ttgxvar;

import freetype;

struct GX_BlendRec {
  FT_UInt num_axis;
  FT_Fixed* coords;
  FT_Fixed* normalizedcoords;

  FT_MM_Var* mmvar;
  FT_Offset mmvar_len;

  FT_Fixed* normalized_stylecoords;

  FT_Bool avar_loaded;
  GX_AVarTable avar_table;

  FT_Bool hvar_loaded;
  FT_Bool hvar_checked;
  FT_Error hvar_error;
  GX_HVVarTable hvar_table;

  FT_Bool vvar_loaded;
  FT_Bool vvar_checked;
  FT_Error vvar_error;
  GX_HVVarTable vvar_table;

  GX_MVarTable mvar_table;

  FT_UInt tuplecount;
  FT_Fixed* tuplecoords;
  FT_Fixed* tuplescalars;

  FT_UInt gv_glyphcnt;
  FT_ULong* glyphoffsets;

  FT_ULong gvar_size;
}

struct GX_HVVarTable {
  GX_ItemVarStoreRec itemStore;
  GX_DeltaSetIdxMapRec widthMap;
}

struct GX_DeltaSetIdxMapRec {
  FT_ULong mapCount;
  FT_UInt* outerIndex;
  FT_UInt* innerIndex;
}

struct GX_MvarTableRec {
  FT_UShort valueCount;

  GX_ItemVarStoreRec itemStore;
  GX_Value values;
}

struct GX_ItemVarStoreRec {
  FT_UInt dataCount;
  GX_ItemVarData varData;

  FT_UShort axisCount;
  FT_UInt regionCount;
  GX_VarRegion varRegionList;
}

struct GX_VarRegionRec {
  GX_AxisCoords axisList;
}

struct GX_ValueRec {
  FT_ULong tag;
  FT_UShort outerIndex;
  FT_UShort innerIndex;

  FT_Short unmodified;
}

struct GX_AxisCoordsRec {
  FT_Fixed startCoord;
  FT_Fixed peakCoord;
  FT_Fixed endCoord;
}

struct GX_ItemVarDataRec {
  FT_UInt itemCount;
  FT_UInt regionIdxCount;
  FT_UInt* regionIndices;

  FT_Byte* deltaSet;

  FT_UShort wordDeltaCount;

  FT_Bool longWords;
}

struct GX_AVarTableRec {
  GX_AVarSegment avar_segment;
  GX_ItemVarStoreRec itemStore;
  GX_DeltaSetIdxMapRec axisMap;
}

struct GX_AvarSegmentRec {
  FT_UShort pairCount;
  GX_AVarCorrespondence correspondence;
}

struct GX_AVarCorrespondenceRec {
  FT_Fixed fromCoord;
  FT_Fixed toCoord;
}

alias GX_Blend = GX_BlendRec*;
alias GX_AVarCorrespondence = GX_AVarCorrespondenceRec*;
alias GX_AVarSegment = GX_AvarSegmentRec*;
alias GX_AVarTable = GX_AVarTableRec*;
alias GX_ItemVarData = GX_ItemVarDataRec*;
alias GX_AxisCoords = GX_AxisCoordsRec*;
alias GX_Value = GX_ValueRec*;
alias GX_VarRegion = GX_VarRegionRec*;
alias GX_ItemVarStore = GX_ItemVarStoreRec*;
alias GX_DeltaSetIdxMap = GX_DeltaSetIdxMapRec*;
alias GX_MVarTable = GX_MvarTableRec*;
