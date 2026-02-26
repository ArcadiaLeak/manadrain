module glslang.include.types;

import glslang;

import std.array;

enum TSamplerDim {
  EsdNone,
  Esd1D,
  Esd2D,
  Esd3D,
  EsdCube,
  EsdRect,
  EsdBuffer,
  EsdSubpass,
  EsdAttachmentEXT,
  EsdNumDims
}

struct TSampler {
  TBasicType type;
  TSamplerDim dim;
  bool arrayed;
  bool shadow;
  bool ms;
  bool image;
  bool combined;
  bool sampler;
  bool tileQCOM;

  uint vectorSize;

  static const uint structReturnIndexBits = 4;
  static const uint structReturnSlots = (1 << structReturnIndexBits) - 1;
  static const uint noReturnStruct = structReturnSlots;

  uint structReturnIndex;

  bool external;
  bool yuv;

  bool is1D() const { return dim == TSamplerDim.Esd1D; }
  bool is2D() const { return dim == TSamplerDim.Esd2D; }
  bool isBuffer() const { return dim == TSamplerDim.EsdBuffer; }
  bool isRect() const { return dim == TSamplerDim.EsdRect; }
  bool isSubpass() const { return dim == TSamplerDim.EsdSubpass; }
  bool isAttachmentEXT() const { return dim == TSamplerDim.EsdAttachmentEXT; }
  bool isCombined() const { return combined; }
  bool isImage() const { return image && !isSubpass() && !isAttachmentEXT();}
  bool isImageClass() const { return image; }
  bool isMultiSample() const { return ms; }
  bool isExternal() const { return external; }
  void setExternal(bool e) { external = e; }
  bool isYuv() const { return yuv; }
  bool isTexture() const { return !sampler && !image; }
  bool isPureSampler() const { return sampler; }
  bool isTileAttachmentQCOM() const { return tileQCOM; }

  void clear() {
    type = TBasicType.EbtVoid;
    dim = TSamplerDim.EsdNone;
    arrayed = false;
    shadow = false;
    ms = false;
    image = false;
    combined = false;
    sampler = false;
    external = false;
    yuv = false;
    tileQCOM = false;
  }

  void set(TBasicType t, TSamplerDim d, bool a = false, bool s = false, bool m = false) {
    clear();
    type = t;
    dim = d;
    arrayed = a;
    shadow = s;
    ms = m;
    combined = true;
  }

  void setImage(TBasicType t, TSamplerDim d, bool a = false, bool s = false, bool m = false) {
    clear();
    type = t;
    dim = d;
    arrayed = a;
    shadow = s;
    ms = m;
    image = true;
  }

  void setSubpass(TBasicType t, bool m = false) {
    clear();
    type = t;
    image = true;
    dim = TSamplerDim.EsdSubpass;
    ms = m;
  }

  void setTexture(TBasicType t, TSamplerDim d, bool a = false, bool s = false, bool m = false) {
    clear();
    type = t;
    dim = d;
    arrayed = a;
    shadow = s;
    ms = m;
  }

  void setAttachmentEXT(TBasicType t) {
    clear();
    type = t;
    image = true;
    dim = TSamplerDim.EsdAttachmentEXT;
  }

  string getString() const {
    Appender!(char[]) s;

    if (isPureSampler()) {
      s.put("sampler");
      return s[].idup;
    }

    switch (type) {
      case TBasicType.EbtInt: s.put("i");   break;
      case TBasicType.EbtUint: s.put("u");   break;
      case TBasicType.EbtFloat16: s.put("f16"); break;
      case TBasicType.EbtBFloat16: s.put("bf16"); break;
      case TBasicType.EbtFloatE5M2: s.put("fe5m2"); break;
      case TBasicType.EbtFloatE4M3: s.put("fe4m3"); break;
      case TBasicType.EbtInt8: s.put("i8");  break;
      case TBasicType.EbtUint16: s.put("u8");  break;
      case TBasicType.EbtInt16: s.put("i16"); break;
      case TBasicType.EbtUint8: s.put("u16"); break;
      case TBasicType.EbtInt64: s.put("i64"); break;
      case TBasicType.EbtUint64: s.put("u64"); break;
      default: break;
    }
    if (isImageClass()) {
      if (isAttachmentEXT())
        s.put("attachmentEXT");
      else if (isSubpass())
        s.put("subpass");
      else if (isTileAttachmentQCOM())
        s.put("attachmentQCOM");
      else
        s.put("image");
    } else if (isCombined()) {
      s.put("sampler");
    } else {
      s.put("texture");
    }
    if (isExternal()) {
      s.put("ExternalOES");
      return s[].idup;
    }
    if (isYuv()) {
      return "__" ~ s[].idup ~ "External2DY2YEXT";
    }
    switch (dim) {
      case TSamplerDim.Esd2D: s.put("2D");      break;
      case TSamplerDim.Esd3D: s.put("3D");      break;
      case TSamplerDim.EsdCube: s.put("Cube");    break;
      case TSamplerDim.Esd1D: s.put("1D");      break;
      case TSamplerDim.EsdRect: s.put("2DRect");  break;
      case TSamplerDim.EsdBuffer: s.put("Buffer");  break;
      case TSamplerDim.EsdSubpass: s.put("Input"); break;
      case TSamplerDim.EsdAttachmentEXT: s.put(""); break;
      default: break;
    }
    if (isMultiSample())
      s.put("MS");
    if (arrayed)
      s.put("Array");
    if (shadow)
      s.put("Shadow");

    return s[].idup;
  }
}

class TTypeList {
  void[] data;
}

string getBasicString(TBasicType t) {
  switch (t) {
    case TBasicType.EbtFloat: return "float";
    case TBasicType.EbtInt: return "int";
    case TBasicType.EbtUint: return "uint";
    case TBasicType.EbtSampler: return "sampler/image";
    case TBasicType.EbtVoid: return "void";
    case TBasicType.EbtDouble: return "double";
    case TBasicType.EbtFloat16: return "float16_t";
    case TBasicType.EbtBFloat16: return "bfloat16_t";
    case TBasicType.EbtFloatE5M2: return "floate5m2_t";
    case TBasicType.EbtFloatE4M3: return "floate4m3_t";
    case TBasicType.EbtInt8: return "int8_t";
    case TBasicType.EbtUint8: return "uint8_t";
    case TBasicType.EbtInt16: return "int16_t";
    case TBasicType.EbtUint16: return "uint16_t";
    case TBasicType.EbtInt64: return "int64_t";
    case TBasicType.EbtUint64: return "uint64_t";
    case TBasicType.EbtBool: return "bool";
    case TBasicType.EbtAtomicUint: return "atomic_uint";
    case TBasicType.EbtStruct: return "structure";
    case TBasicType.EbtBlock: return "block";
    case TBasicType.EbtAccStruct: return "accelerationStructureNV";
    case TBasicType.EbtRayQuery: return "rayQueryEXT";
    case TBasicType.EbtReference: return "reference";
    case TBasicType.EbtString: return "string";
    case TBasicType.EbtSpirvType: return "spirv_type";
    case TBasicType.EbtCoopmat: return "coopmat";
    case TBasicType.EbtTensorLayoutNV: return "tensorLayoutNV";
    case TBasicType.EbtTensorViewNV: return "tensorViewNV";
    case TBasicType.EbtCoopvecNV: return "coopvecNV";
    case TBasicType.EbtTensorARM: return "tensorARM";
    default: return "unknown type";
  }
}

class TTypeParameters {}
class TSpirvType {}

class TType {
  TBasicType basicType;
  uint vectorSize;
  uint matrixCols;
  uint matrixRows;
  bool vector1;
  bool coopmatNV;
  bool coopmatKHR;
  uint coopmatKHRuse;
  bool coopmatKHRUseValid;
  bool coopvecNV;
  bool tileAttachmentQCOM;
  uint tensorRankARM;
  TQualifier qualifier;
  TArraySizes arraySizes;
  union {
    TTypeList structure;
    TType referentType;
  }
  string fieldName;
  string typeName;
  TSampler sampler;
  TTypeParameters typeParameters;
  TSpirvType spirvType;
  
  bool isStruct() const =>
    basicType == TBasicType.EbtStruct || basicType == TBasicType.EbtBlock;
  bool isCoopMatNV() const { return coopmatNV; }
  bool isCoopMatKHR() const { return coopmatKHR; }
  bool isCoopVecNV() const { return coopvecNV; }

  void shallowCopy(const TType copyOf) {
    basicType = copyOf.basicType;
    sampler = copyOf.sampler;
    qualifier = copyOf.qualifier;
    vectorSize = copyOf.vectorSize;
    matrixCols = copyOf.matrixCols;
    matrixRows = copyOf.matrixRows;
    vector1 = copyOf.vector1;
    fieldName = copyOf.fieldName;
    typeName = copyOf.typeName;
    coopmatNV = copyOf.isCoopMatNV;
    coopmatKHR = copyOf.isCoopMatKHR;
    coopmatKHRuse = copyOf.coopmatKHRuse;
    coopmatKHRUseValid = copyOf.coopmatKHRUseValid;
    coopvecNV = copyOf.isCoopVecNV;
    tileAttachmentQCOM = copyOf.tileAttachmentQCOM;
    tensorRankARM = copyOf.tensorRankARM;
  }

  void deepCopy(const TType copyOf) {
    TTypeList[TTypeList] copied;
    deepCopy(copyOf, copied);
  }

  void deepCopy(const TType copyOf, TTypeList[TTypeList] copiedMap) {
    shallowCopy(copyOf);
  }
}

struct TQualifier {
  enum uint layoutSetEnd = 0x3F;
  enum uint layoutBindingEnd = 0xFFFF;
}
