module glslang.machine_independent.initialize;
import glslang;

import std.algorithm.searching;
import std.container.dlist;
import std.conv;
import std.string;

enum string[] TypeString = [
  "bool", "bvec2", "bvec3", "bvec4",
  "float", "vec2", "vec3", "vec4",
  "int", "ivec2", "ivec3", "ivec4",
  "uint", "uvec2", "uvec3", "uvec4",
];
enum int TypeStringCount = TypeString.length;
enum int TypeStringRowShift = 2;
enum int TypeStringColumnMask = (1 << TypeStringRowShift) - 1;
enum int TypeStringScalarMask = ~TypeStringColumnMask;

enum ArgClass ClassV1FIOCVN = ArgClass.ClassV1 | ArgClass.ClassFIO | ArgClass.ClassCVN;
enum ArgClass ClassBNS = ArgClass.ClassB | ArgClass.ClassNS;
enum ArgClass ClassRSNS = ArgClass.ClassRS | ArgClass.ClassNS;

enum ArgType TypeFI = ArgType.TypeF | ArgType.TypeI;
enum ArgType TypeFIB = ArgType.TypeF | ArgType.TypeI | ArgType.TypeB;
enum ArgType TypeIU = ArgType.TypeI | ArgType.TypeU;

enum bool ForwardCompatibility = false;

enum BuiltInFunction[] BaseFunctions = [
  BuiltInFunction(TOperator.EOpRadians, "radians", 1, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpDegrees, "degrees", 1, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpSin, "sin", 1, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpTan, "tan", 1, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpAsin, "asin", 1, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpAcos, "acos", 1, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpAtan, "atan", 2, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpAtan, "atan", 1, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpPow, "pow", 2, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpExp, "exp", 1, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpLog, "log", 1, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpExp2, "exp2", 1, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpLog2, "log2", 1, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpSqrt, "sqrt", 1, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpInverseSqrt, "inversesqrt", 1, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpAbs, "abs", 1, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpSign, "sign", 1, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpFloor, "floor", 1, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpCeil, "ceil", 1, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpFract, "fract", 1, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpMod, "mod", 2, ArgType.TypeF, ArgClass.ClassLS, []),
  BuiltInFunction(TOperator.EOpMin, "min", 2, ArgType.TypeF, ArgClass.ClassLS, []),
  BuiltInFunction(TOperator.EOpMax, "max", 2, ArgType.TypeF, ArgClass.ClassLS, []),
  BuiltInFunction(TOperator.EOpClamp, "clamp", 3, ArgType.TypeF, ArgClass.ClassLS2, []),
  BuiltInFunction(TOperator.EOpMix, "mix", 3, ArgType.TypeF, ArgClass.ClassLS, []),
  BuiltInFunction(TOperator.EOpStep, "step", 2, ArgType.TypeF, ArgClass.ClassFS, []),
  BuiltInFunction(TOperator.EOpSmoothStep, "smoothstep", 3, ArgType.TypeF, ArgClass.ClassFS2, []),
  BuiltInFunction(TOperator.EOpNormalize, "normalize", 1, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpFaceForward, "faceforward", 3, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpReflect, "reflect", 2, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpRefract, "refract", 3, ArgType.TypeF, ArgClass.ClassXLS, []),
  BuiltInFunction(TOperator.EOpLength, "length", 1, ArgType.TypeF, ArgClass.ClassRS, []),
  BuiltInFunction(TOperator.EOpDistance, "distance", 2, ArgType.TypeF, ArgClass.ClassRS, []),
  BuiltInFunction(TOperator.EOpDot, "dot", 2, ArgType.TypeF, ArgClass.ClassRS, []),
  BuiltInFunction(TOperator.EOpCross, "cross", 2, ArgType.TypeF, ArgClass.ClassV3, []),
  BuiltInFunction(TOperator.EOpLessThan, "lessThan", 2, TypeFI, ClassBNS, []),
  BuiltInFunction(TOperator.EOpLessThanEqual, "lessThanEqual", 2, TypeFI, ClassBNS, []),
  BuiltInFunction(TOperator.EOpGreaterThan, "greaterThan", 2, TypeFI, ClassBNS, []),
  BuiltInFunction(TOperator.EOpGreaterThanEqual, "greaterThanEqual", 2, TypeFI, ClassBNS, []),
  BuiltInFunction(TOperator.EOpVectorEqual, "equal", 2, TypeFIB, ClassBNS, []),
  BuiltInFunction(TOperator.EOpAny, "any", 1, ArgType.TypeB, ClassRSNS, []),
  BuiltInFunction(TOperator.EOpAll, "all", 1, ArgType.TypeB, ClassRSNS, []),
  BuiltInFunction(TOperator.EOpVectorLogicalNot, "not", 1, ArgType.TypeB, ArgClass.ClassNS, []),
  BuiltInFunction(TOperator.EOpSinh, "sinh", 1, ArgType.TypeF, ArgClass.ClassRegular, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpCosh, "cosh", 1, ArgType.TypeF, ArgClass.ClassRegular, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpTanh, "tanh", 1, ArgType.TypeF, ArgClass.ClassRegular, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpAsinh, "asinh", 1, ArgType.TypeF, ArgClass.ClassRegular, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpAcosh, "acosh", 1, ArgType.TypeF, ArgClass.ClassRegular, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpAtanh, "atanh", 1, ArgType.TypeF, ArgClass.ClassRegular, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpAbs, "abs", 1, ArgType.TypeI, ArgClass.ClassRegular, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpSign, "sign", 1, ArgType.TypeI, ArgClass.ClassRegular, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpTrunc, "trunc", 1, ArgType.TypeF, ArgClass.ClassRegular, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpRound, "round", 1, ArgType.TypeF, ArgClass.ClassRegular, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpRoundEven, "roundEven", 1, ArgType.TypeF, ArgClass.ClassRegular, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpModf, "modf", 2, ArgType.TypeF, ArgClass.ClassLO, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpMin, "min", 2, TypeIU, ArgClass.ClassLS, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpMax, "max", 2, TypeIU, ArgClass.ClassLS, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpClamp, "clamp", 3, TypeIU, ArgClass.ClassLS2, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpMix, "mix", 3, ArgType.TypeF, ArgClass.ClassLB, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpIsInf, "isinf", 1, ArgType.TypeF, ArgClass.ClassB, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpIsNan, "isnan", 1, ArgType.TypeF, ArgClass.ClassB, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpLessThan, "lessThan", 2, ArgType.TypeU, ClassBNS, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpLessThanEqual, "lessThanEqual", 2, ArgType.TypeU, ClassBNS, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpGreaterThan, "greaterThan", 2, ArgType.TypeU, ClassBNS, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpGreaterThanEqual, "greaterThanEqual", 2, ArgType.TypeU, ClassBNS, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpVectorEqual, "equal", 2, ArgType.TypeU, ClassBNS, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpVectorNotEqual, "notEqual", 2, ArgType.TypeU, ClassBNS, Es300Desktop130Version),
  BuiltInFunction(TOperator.EOpAtomicAdd, "atomicAdd", 2, TypeIU, ClassV1FIOCVN, Es310Desktop400Version),
  BuiltInFunction(TOperator.EOpAtomicMin, "atomicMin", 2, TypeIU, ClassV1FIOCVN, Es310Desktop400Version),
  BuiltInFunction(TOperator.EOpAtomicMax, "atomicMax", 2, TypeIU, ClassV1FIOCVN, Es310Desktop400Version),
  BuiltInFunction(TOperator.EOpAtomicAnd, "atomicAnd", 2, TypeIU, ClassV1FIOCVN, Es310Desktop400Version),
  BuiltInFunction(TOperator.EOpAtomicOr, "atomicOr", 2, TypeIU, ClassV1FIOCVN, Es310Desktop400Version),
  BuiltInFunction(TOperator.EOpAtomicExchange, "atomicExchange", 2, TypeIU, ClassV1FIOCVN, Es310Desktop400Version),
  BuiltInFunction(TOperator.EOpAtomicCompSwap, "atomicCompSwap", 3, TypeIU, ClassV1FIOCVN, Es310Desktop400Version),
  BuiltInFunction(TOperator.EOpMix, "mix", 3, ArgType.TypeB, ArgClass.ClassRegular, Es310Desktop450Version),
  BuiltInFunction(TOperator.EOpMix, "mix", 3, TypeIU, ArgClass.ClassLB, Es310Desktop450Version)
];

enum BuiltInFunction[] DerivativeFunctions = [
  BuiltInFunction(TOperator.EOpDPdx, "dFdx", 1, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpDPdy, "dFdy", 1, ArgType.TypeF, ArgClass.ClassRegular, []),
  BuiltInFunction(TOperator.EOpFwidth, "fwidth", 1, ArgType.TypeF, ArgClass.ClassRegular, [])
];

enum EProfile EEsProfile =
  EProfile(ES_PROFILE: 1);
enum EProfile EDesktopProfile =
  EProfile(NO_PROFILE: 1, CORE_PROFILE: 1, COMPATIBILITY_PROFILE: 1);

enum Versioning[] Es300Desktop130Version = [
  Versioning(EEsProfile, 0, 300, 0, null),
  Versioning(EDesktopProfile, 0, 130, 0, null),
];

enum Versioning[] Es310Desktop400Version = [
  Versioning(EEsProfile, 0, 310, 0, null),
  Versioning(EDesktopProfile, 0, 400, 0, null),
];

enum Versioning[] Es310Desktop450Version = [
  Versioning(EEsProfile, 0, 310, 0, null),
  Versioning(EDesktopProfile, 0, 450, 0, null),
];

 bool logicalOp(string op) => canFind(op, "Or") || canFind(op, "And") ||
  (canFind(op, "Xor") && !canFind(op, "ShuffleXor"));

bool mathOp(string op) => canFind(op, "Add") || canFind(op, "Mul") ||
  canFind(op, "Min") || canFind(op, "Max");

enum string[] subgroupOps = [
  "bool subgroupAllEqual(%1$s);\n",
  "%1$s subgroupBroadcast(%1$s, uint);\n",
  "%1$s subgroupBroadcastFirst(%1$s);\n",
  "%1$s subgroupShuffle(%1$s, uint);\n",
  "%1$s subgroupShuffleXor(%1$s, uint);\n",
  "%1$s subgroupShuffleUp(%1$s, uint delta);\n",
  "%1$s subgroupShuffleDown(%1$s, uint delta);\n",
  "%1$s subgroupRotate(%1$s, uint);\n",
  "%1$s subgroupClusteredRotate(%1$s, uint, uint);\n",
  "%1$s subgroupAdd(%1$s);\n",
  "%1$s subgroupMul(%1$s);\n",
  "%1$s subgroupMin(%1$s);\n",
  "%1$s subgroupMax(%1$s);\n",
  "%1$s subgroupAnd(%1$s);\n",
  "%1$s subgroupOr(%1$s);\n",
  "%1$s subgroupXor(%1$s);\n",
  "%1$s subgroupInclusiveAdd(%1$s);\n",
  "%1$s subgroupInclusiveMul(%1$s);\n",
  "%1$s subgroupInclusiveMin(%1$s);\n",
  "%1$s subgroupInclusiveMax(%1$s);\n",
  "%1$s subgroupInclusiveAnd(%1$s);\n",
  "%1$s subgroupInclusiveOr(%1$s);\n",
  "%1$s subgroupInclusiveXor(%1$s);\n",
  "%1$s subgroupExclusiveAdd(%1$s);\n",
  "%1$s subgroupExclusiveMul(%1$s);\n",
  "%1$s subgroupExclusiveMin(%1$s);\n",
  "%1$s subgroupExclusiveMax(%1$s);\n",
  "%1$s subgroupExclusiveAnd(%1$s);\n",
  "%1$s subgroupExclusiveOr(%1$s);\n",
  "%1$s subgroupExclusiveXor(%1$s);\n",
  "%1$s subgroupClusteredAdd(%1$s, uint);\n",
  "%1$s subgroupClusteredMul(%1$s, uint);\n",
  "%1$s subgroupClusteredMin(%1$s, uint);\n",
  "%1$s subgroupClusteredMax(%1$s, uint);\n",
  "%1$s subgroupClusteredAnd(%1$s, uint);\n",
  "%1$s subgroupClusteredOr(%1$s, uint);\n",
  "%1$s subgroupClusteredXor(%1$s, uint);\n",
  "%1$s subgroupQuadBroadcast(%1$s, uint);\n",
  "%1$s subgroupQuadSwapHorizontal(%1$s);\n",
  "%1$s subgroupQuadSwapVertical(%1$s);\n",
  "%1$s subgroupQuadSwapDiagonal(%1$s);\n",
  "uvec4 subgroupPartitionNV(%1$s);\n",
  "%1$s subgroupPartitionedAddNV(%1$s, uvec4 ballot);\n",
  "%1$s subgroupPartitionedMulNV(%1$s, uvec4 ballot);\n",
  "%1$s subgroupPartitionedMinNV(%1$s, uvec4 ballot);\n",
  "%1$s subgroupPartitionedMaxNV(%1$s, uvec4 ballot);\n",
  "%1$s subgroupPartitionedAndNV(%1$s, uvec4 ballot);\n",
  "%1$s subgroupPartitionedOrNV(%1$s, uvec4 ballot);\n",
  "%1$s subgroupPartitionedXorNV(%1$s, uvec4 ballot);\n",
  "%1$s subgroupPartitionedInclusiveAddNV(%1$s, uvec4 ballot);\n",
  "%1$s subgroupPartitionedInclusiveMulNV(%1$s, uvec4 ballot);\n",
  "%1$s subgroupPartitionedInclusiveMinNV(%1$s, uvec4 ballot);\n",
  "%1$s subgroupPartitionedInclusiveMaxNV(%1$s, uvec4 ballot);\n",
  "%1$s subgroupPartitionedInclusiveAndNV(%1$s, uvec4 ballot);\n",
  "%1$s subgroupPartitionedInclusiveOrNV(%1$s, uvec4 ballot);\n",
  "%1$s subgroupPartitionedInclusiveXorNV(%1$s, uvec4 ballot);\n",
  "%1$s subgroupPartitionedExclusiveAddNV(%1$s, uvec4 ballot);\n",
  "%1$s subgroupPartitionedExclusiveMulNV(%1$s, uvec4 ballot);\n",
  "%1$s subgroupPartitionedExclusiveMinNV(%1$s, uvec4 ballot);\n",
  "%1$s subgroupPartitionedExclusiveMaxNV(%1$s, uvec4 ballot);\n",
  "%1$s subgroupPartitionedExclusiveAndNV(%1$s, uvec4 ballot);\n",
  "%1$s subgroupPartitionedExclusiveOrNV(%1$s, uvec4 ballot);\n",
  "%1$s subgroupPartitionedExclusiveXorNV(%1$s, uvec4 ballot);\n",
];

enum string[] floatTypes = [
  "float", "vec2", "vec3", "vec4",
  "float16_t", "f16vec2", "f16vec3", "f16vec4"
];

enum string[] doubleTypes = [
  "double", "dvec2", "dvec3", "dvec4"
];

enum string[] intTypes = [
  "int8_t", "i8vec2", "i8vec3", "i8vec4", 
  "int16_t", "i16vec2", "i16vec3", "i16vec4", 
  "int", "ivec2", "ivec3", "ivec4", 
  "int64_t", "i64vec2", "i64vec3", "i64vec4", 
  "uint8_t", "u8vec2", "u8vec3", "u8vec4", 
  "uint16_t", "u16vec2", "u16vec3", "u16vec4", 
  "uint", "uvec2", "uvec3", "uvec4", 
  "uint64_t", "u64vec2", "u64vec3", "u64vec4"
];

enum string[] boolTypes = [
  "bool", "bvec2", "bvec3", "bvec4"
];

class TBuiltInParseables {
  struct StageBuiltins {
    DList!string STAGE_VERTEX;
    DList!string STAGE_TESSCONTROL;
    DList!string STAGE_TESSEVALUATION;
    DList!string STAGE_GEOMETRY;
    DList!string STAGE_FRAGMENT;
    DList!string STAGE_COMPUTE;
    DList!string STAGE_RAYGEN;
    DList!string STAGE_INTERSECT;
    DList!string STAGE_ANYHIT;
    DList!string STAGE_CLOSESTHIT;
    DList!string STAGE_MISS;
    DList!string STAGE_CALLABLE;
    DList!string STAGE_TASK;
    DList!string STAGE_MESH;
  }

  DList!string commonBuiltins;
  StageBuiltins stageBuiltins;

  abstract void initialize(
    int version_, EProfile, in SpvVersion spvVersion);
  abstract void initialize(
    int version_, EProfile, in SpvVersion spvVersion, EShLanguage);

  string getCommonString() => commonBuiltins[].join;
  
}

class TBuiltIns : TBuiltInParseables {
  string[5] postfixes;
  string[TBasicType.EbtNumTypes] prefixes;
  int[TSamplerDim.EsdNumDims] dimMap;

  this() {
    prefixes[TBasicType.EbtFloat] =  "";
    prefixes[TBasicType.EbtInt] = "i";
    prefixes[TBasicType.EbtUint] = "u";
    prefixes[TBasicType.EbtFloat16] = "f16";
    prefixes[TBasicType.EbtInt8] = "i8";
    prefixes[TBasicType.EbtUint8] = "u8";
    prefixes[TBasicType.EbtInt16] = "i16";
    prefixes[TBasicType.EbtUint16] = "u16";
    prefixes[TBasicType.EbtInt64] = "i64";
    prefixes[TBasicType.EbtUint64] = "u64";

    postfixes[2] = "2";
    postfixes[3] = "3";
    postfixes[4] = "4";

    dimMap[TSamplerDim.Esd2D] = 2;
    dimMap[TSamplerDim.Esd3D] = 3;
    dimMap[TSamplerDim.EsdCube] = 3;
    dimMap[TSamplerDim.Esd1D] = 1;
    dimMap[TSamplerDim.EsdRect] = 2;
    dimMap[TSamplerDim.EsdBuffer] = 1;
    dimMap[TSamplerDim.EsdSubpass] = 2;
    dimMap[TSamplerDim.EsdAttachmentEXT] = 2;
  }

  void addTabledBuiltins(
    int version_, EProfile profile,
    in SpvVersion spvVersion
  ) {
    auto forEachFunction(ref DList!string decls, const(BuiltInFunction[]) functions) {
      foreach (const ref fn; functions) {
        if (ValidVersion(fn, version_, profile, spvVersion))
          AddTabledBuiltin(decls, fn);
      }
    }

    forEachFunction(commonBuiltins, BaseFunctions);
    forEachFunction(stageBuiltins.STAGE_FRAGMENT, DerivativeFunctions);

    if ((profile == EProfile(ES_PROFILE: 1) && version_ >= 320) ||
      (profile != EProfile(ES_PROFILE: 1) && version_ >= 450))
      forEachFunction(stageBuiltins.STAGE_COMPUTE, DerivativeFunctions);
  }

  override void initialize(
    int version_, EProfile profile,
    in SpvVersion spvVersion
  ) {
    addTabledBuiltins(version_, profile, spvVersion);

    string derivativeControls = (
      "float dFdxFine(float p);" ~
      "vec2 dFdxFine(vec2 p);" ~
      "vec3 dFdxFine(vec3 p);" ~
      "vec4 dFdxFine(vec4 p);" ~

      "float dFdyFine(float p);" ~
      "vec2 dFdyFine(vec2 p);" ~
      "vec3 dFdyFine(vec3 p);" ~
      "vec4 dFdyFine(vec4 p);" ~

      "float fwidthFine(float p);" ~
      "vec2 fwidthFine(vec2 p);" ~
      "vec3 fwidthFine(vec3 p);" ~
      "vec4 fwidthFine(vec4 p);" ~

      "float dFdxCoarse(float p);" ~
      "vec2 dFdxCoarse(vec2 p);" ~
      "vec3 dFdxCoarse(vec3 p);" ~
      "vec4 dFdxCoarse(vec4 p);" ~

      "float dFdyCoarse(float p);" ~
      "vec2 dFdyCoarse(vec2 p);" ~
      "vec3 dFdyCoarse(vec3 p);" ~
      "vec4 dFdyCoarse(vec4 p);" ~

      "float fwidthCoarse(float p);" ~
      "vec2 fwidthCoarse(vec2 p);" ~
      "vec3 fwidthCoarse(vec3 p);" ~
      "vec4 fwidthCoarse(vec4 p);"
    );

    string derivativesAndControl16bits = (
      "float16_t dFdx(float16_t);" ~
      "f16vec2 dFdx(f16vec2);" ~
      "f16vec3 dFdx(f16vec3);" ~
      "f16vec4 dFdx(f16vec4);" ~

      "float16_t dFdy(float16_t);" ~
      "f16vec2 dFdy(f16vec2);" ~
      "f16vec3 dFdy(f16vec3);" ~
      "f16vec4 dFdy(f16vec4);" ~

      "float16_t dFdxFine(float16_t);" ~
      "f16vec2 dFdxFine(f16vec2);" ~
      "f16vec3 dFdxFine(f16vec3);" ~
      "f16vec4 dFdxFine(f16vec4);" ~

      "float16_t dFdyFine(float16_t);" ~
      "f16vec2 dFdyFine(f16vec2);" ~
      "f16vec3 dFdyFine(f16vec3);" ~
      "f16vec4 dFdyFine(f16vec4);" ~

      "float16_t dFdxCoarse(float16_t);" ~
      "f16vec2 dFdxCoarse(f16vec2);" ~
      "f16vec3 dFdxCoarse(f16vec3);" ~
      "f16vec4 dFdxCoarse(f16vec4);" ~

      "float16_t dFdyCoarse(float16_t);" ~
      "f16vec2 dFdyCoarse(f16vec2);" ~
      "f16vec3 dFdyCoarse(f16vec3);" ~
      "f16vec4 dFdyCoarse(f16vec4);" ~

      "float16_t fwidth(float16_t);" ~
      "f16vec2 fwidth(f16vec2);" ~
      "f16vec3 fwidth(f16vec3);" ~
      "f16vec4 fwidth(f16vec4);" ~

      "float16_t fwidthFine(float16_t);" ~
      "f16vec2 fwidthFine(f16vec2);" ~
      "f16vec3 fwidthFine(f16vec3);" ~
      "f16vec4 fwidthFine(f16vec4);" ~

      "float16_t fwidthCoarse(float16_t);" ~
      "f16vec2 fwidthCoarse(f16vec2);" ~
      "f16vec3 fwidthCoarse(f16vec3);" ~
      "f16vec4 fwidthCoarse(f16vec4);"
    );

    string derivativesAndControl64bits = (
      "float64_t dFdx(float64_t);" ~
      "f64vec2 dFdx(f64vec2);" ~
      "f64vec3 dFdx(f64vec3);" ~
      "f64vec4 dFdx(f64vec4);" ~

      "float64_t dFdy(float64_t);" ~
      "f64vec2 dFdy(f64vec2);" ~
      "f64vec3 dFdy(f64vec3);" ~
      "f64vec4 dFdy(f64vec4);" ~

      "float64_t dFdxFine(float64_t);" ~
      "f64vec2 dFdxFine(f64vec2);" ~
      "f64vec3 dFdxFine(f64vec3);" ~
      "f64vec4 dFdxFine(f64vec4);" ~

      "float64_t dFdyFine(float64_t);" ~
      "f64vec2 dFdyFine(f64vec2);" ~
      "f64vec3 dFdyFine(f64vec3);" ~
      "f64vec4 dFdyFine(f64vec4);" ~

      "float64_t dFdxCoarse(float64_t);" ~
      "f64vec2 dFdxCoarse(f64vec2);" ~
      "f64vec3 dFdxCoarse(f64vec3);" ~
      "f64vec4 dFdxCoarse(f64vec4);" ~

      "float64_t dFdyCoarse(float64_t);" ~
      "f64vec2 dFdyCoarse(f64vec2);" ~
      "f64vec3 dFdyCoarse(f64vec3);" ~
      "f64vec4 dFdyCoarse(f64vec4);" ~

      "float64_t fwidth(float64_t);" ~
      "f64vec2 fwidth(f64vec2);" ~
      "f64vec3 fwidth(f64vec3);" ~
      "f64vec4 fwidth(f64vec4);" ~

      "float64_t fwidthFine(float64_t);" ~
      "f64vec2 fwidthFine(f64vec2);" ~
      "f64vec3 fwidthFine(f64vec3);" ~
      "f64vec4 fwidthFine(f64vec4);" ~

      "float64_t fwidthCoarse(float64_t);" ~
      "f64vec2 fwidthCoarse(f64vec2);" ~
      "f64vec3 fwidthCoarse(f64vec3);" ~
      "f64vec4 fwidthCoarse(f64vec4);"
    );

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 150) {
      commonBuiltins ~= (
        "double sqrt(double);" ~
        "dvec2 sqrt(dvec2);" ~
        "dvec3 sqrt(dvec3);" ~
        "dvec4 sqrt(dvec4);" ~

        "double inversesqrt(double);" ~
        "dvec2 inversesqrt(dvec2);" ~
        "dvec3 inversesqrt(dvec3);" ~
        "dvec4 inversesqrt(dvec4);" ~

        "double abs(double);" ~
        "dvec2 abs(dvec2);" ~
        "dvec3 abs(dvec3);" ~
        "dvec4 abs(dvec4);" ~

        "double sign(double);" ~
        "dvec2 sign(dvec2);" ~
        "dvec3 sign(dvec3);" ~
        "dvec4 sign(dvec4);" ~

        "double floor(double);" ~
        "dvec2 floor(dvec2);" ~
        "dvec3 floor(dvec3);" ~
        "dvec4 floor(dvec4);" ~

        "double trunc(double);" ~
        "dvec2 trunc(dvec2);" ~
        "dvec3 trunc(dvec3);" ~
        "dvec4 trunc(dvec4);" ~

        "double round(double);" ~
        "dvec2 round(dvec2);" ~
        "dvec3 round(dvec3);" ~
        "dvec4 round(dvec4);" ~

        "double roundEven(double);" ~
        "dvec2 roundEven(dvec2);" ~
        "dvec3 roundEven(dvec3);" ~
        "dvec4 roundEven(dvec4);" ~

        "double ceil(double);" ~
        "dvec2 ceil(dvec2);" ~
        "dvec3 ceil(dvec3);" ~
        "dvec4 ceil(dvec4);" ~

        "double fract(double);" ~
        "dvec2 fract(dvec2);" ~
        "dvec3 fract(dvec3);" ~
        "dvec4 fract(dvec4);" ~

        "double mod(double, double);" ~
        "dvec2 mod(dvec2, double);" ~
        "dvec3 mod(dvec3, double);" ~
        "dvec4 mod(dvec4, double);" ~
        "dvec2 mod(dvec2, dvec2);" ~
        "dvec3 mod(dvec3, dvec3);" ~
        "dvec4 mod(dvec4, dvec4);" ~

        "double modf(double, out double);" ~
        "dvec2 modf(dvec2, out dvec2);" ~
        "dvec3 modf(dvec3, out dvec3);" ~
        "dvec4 modf(dvec4, out dvec4);" ~

        "double min(double, double);" ~
        "dvec2 min(dvec2, double);" ~
        "dvec3 min(dvec3, double);" ~
        "dvec4 min(dvec4, double);" ~
        "dvec2 min(dvec2, dvec2);" ~
        "dvec3 min(dvec3, dvec3);" ~
        "dvec4 min(dvec4, dvec4);" ~

        "double max(double, double);" ~
        "dvec2 max(dvec2, double);" ~
        "dvec3 max(dvec3, double);" ~
        "dvec4 max(dvec4, double);" ~
        "dvec2 max(dvec2, dvec2);" ~
        "dvec3 max(dvec3, dvec3);" ~
        "dvec4 max(dvec4, dvec4);" ~

        "double clamp(double, double, double);" ~
        "dvec2 clamp(dvec2, double, double);" ~
        "dvec3 clamp(dvec3, double, double);" ~
        "dvec4 clamp(dvec4, double, double);" ~
        "dvec2 clamp(dvec2, dvec2, dvec2);" ~
        "dvec3 clamp(dvec3, dvec3, dvec3);" ~
        "dvec4 clamp(dvec4, dvec4, dvec4);" ~

        "double mix(double, double, double);" ~
        "dvec2 mix(dvec2, dvec2, double);" ~
        "dvec3 mix(dvec3, dvec3, double);" ~
        "dvec4 mix(dvec4, dvec4, double);" ~
        "dvec2 mix(dvec2, dvec2, dvec2);" ~
        "dvec3 mix(dvec3, dvec3, dvec3);" ~
        "dvec4 mix(dvec4, dvec4, dvec4);" ~
        "double mix(double, double, bool);" ~
        "dvec2 mix(dvec2, dvec2, bvec2);" ~
        "dvec3 mix(dvec3, dvec3, bvec3);" ~
        "dvec4 mix(dvec4, dvec4, bvec4);" ~

        "double step(double, double);" ~
        "dvec2 step(dvec2, dvec2);" ~
        "dvec3 step(dvec3, dvec3);" ~
        "dvec4 step(dvec4, dvec4);" ~
        "dvec2 step(double, dvec2);" ~
        "dvec3 step(double, dvec3);" ~
        "dvec4 step(double, dvec4);" ~

        "double smoothstep(double, double, double);" ~
        "dvec2 smoothstep(dvec2, dvec2, dvec2);" ~
        "dvec3 smoothstep(dvec3, dvec3, dvec3);" ~
        "dvec4 smoothstep(dvec4, dvec4, dvec4);" ~
        "dvec2 smoothstep(double, double, dvec2);" ~
        "dvec3 smoothstep(double, double, dvec3);" ~
        "dvec4 smoothstep(double, double, dvec4);" ~

        "bool isnan(double);" ~
        "bvec2 isnan(dvec2);" ~
        "bvec3 isnan(dvec3);" ~
        "bvec4 isnan(dvec4);" ~

        "bool isinf(double);" ~
        "bvec2 isinf(dvec2);" ~
        "bvec3 isinf(dvec3);" ~
        "bvec4 isinf(dvec4);" ~

        "double length(double);" ~
        "double length(dvec2);" ~
        "double length(dvec3);" ~
        "double length(dvec4);" ~

        "double distance(double, double);" ~
        "double distance(dvec2, dvec2);" ~
        "double distance(dvec3, dvec3);" ~
        "double distance(dvec4, dvec4);" ~

        "double dot(double, double);" ~
        "double dot(dvec2, dvec2);" ~
        "double dot(dvec3, dvec3);" ~
        "double dot(dvec4, dvec4);" ~

        "dvec3 cross(dvec3, dvec3);" ~

        "double normalize(double);" ~
        "dvec2 normalize(dvec2);" ~
        "dvec3 normalize(dvec3);" ~
        "dvec4 normalize(dvec4);" ~

        "double faceforward(double, double, double);" ~
        "dvec2 faceforward(dvec2, dvec2, dvec2);" ~
        "dvec3 faceforward(dvec3, dvec3, dvec3);" ~
        "dvec4 faceforward(dvec4, dvec4, dvec4);" ~

        "double reflect(double, double);" ~
        "dvec2 reflect(dvec2, dvec2);" ~
        "dvec3 reflect(dvec3, dvec3);" ~
        "dvec4 reflect(dvec4, dvec4);" ~

        "double refract(double, double, double);" ~
        "dvec2 refract(dvec2, dvec2, double);" ~
        "dvec3 refract(dvec3, dvec3, double);" ~
        "dvec4 refract(dvec4, dvec4, double);" ~

        "dmat2 matrixCompMult(dmat2, dmat2);" ~
        "dmat3 matrixCompMult(dmat3, dmat3);" ~
        "dmat4 matrixCompMult(dmat4, dmat4);" ~
        "dmat2x3 matrixCompMult(dmat2x3, dmat2x3);" ~
        "dmat2x4 matrixCompMult(dmat2x4, dmat2x4);" ~
        "dmat3x2 matrixCompMult(dmat3x2, dmat3x2);" ~
        "dmat3x4 matrixCompMult(dmat3x4, dmat3x4);" ~
        "dmat4x2 matrixCompMult(dmat4x2, dmat4x2);" ~
        "dmat4x3 matrixCompMult(dmat4x3, dmat4x3);" ~

        "dmat2 outerProduct(dvec2, dvec2);" ~
        "dmat3 outerProduct(dvec3, dvec3);" ~
        "dmat4 outerProduct(dvec4, dvec4);" ~
        "dmat2x3 outerProduct(dvec3, dvec2);" ~
        "dmat3x2 outerProduct(dvec2, dvec3);" ~
        "dmat2x4 outerProduct(dvec4, dvec2);" ~
        "dmat4x2 outerProduct(dvec2, dvec4);" ~
        "dmat3x4 outerProduct(dvec4, dvec3);" ~
        "dmat4x3 outerProduct(dvec3, dvec4);" ~

        "dmat2 transpose(dmat2);" ~
        "dmat3 transpose(dmat3);" ~
        "dmat4 transpose(dmat4);" ~
        "dmat2x3 transpose(dmat3x2);" ~
        "dmat3x2 transpose(dmat2x3);" ~
        "dmat2x4 transpose(dmat4x2);" ~
        "dmat4x2 transpose(dmat2x4);" ~
        "dmat3x4 transpose(dmat4x3);" ~
        "dmat4x3 transpose(dmat3x4);" ~

        "double determinant(dmat2);" ~
        "double determinant(dmat3);" ~
        "double determinant(dmat4);" ~

        "dmat2 inverse(dmat2);" ~
        "dmat3 inverse(dmat3);" ~
        "dmat4 inverse(dmat4);" ~

        "bvec2 lessThan(dvec2, dvec2);" ~
        "bvec3 lessThan(dvec3, dvec3);" ~
        "bvec4 lessThan(dvec4, dvec4);" ~

        "bvec2 lessThanEqual(dvec2, dvec2);" ~
        "bvec3 lessThanEqual(dvec3, dvec3);" ~
        "bvec4 lessThanEqual(dvec4, dvec4);" ~

        "bvec2 greaterThan(dvec2, dvec2);" ~
        "bvec3 greaterThan(dvec3, dvec3);" ~
        "bvec4 greaterThan(dvec4, dvec4);" ~

        "bvec2 greaterThanEqual(dvec2, dvec2);" ~
        "bvec3 greaterThanEqual(dvec3, dvec3);" ~
        "bvec4 greaterThanEqual(dvec4, dvec4);" ~

        "bvec2 equal(dvec2, dvec2);" ~
        "bvec3 equal(dvec3, dvec3);" ~
        "bvec4 equal(dvec4, dvec4);" ~

        "bvec2 notEqual(dvec2, dvec2);" ~
        "bvec3 notEqual(dvec3, dvec3);" ~
        "bvec4 notEqual(dvec4, dvec4);" ~

        "\n"
      );
    }

    if (profile == EProfile(ES_PROFILE: 1) && version_ >= 310) {
      commonBuiltins ~= (
        "float64_t sqrt(float64_t);" ~
        "f64vec2 sqrt(f64vec2);" ~
        "f64vec3 sqrt(f64vec3);" ~
        "f64vec4 sqrt(f64vec4);" ~

        "float64_t inversesqrt(float64_t);" ~
        "f64vec2 inversesqrt(f64vec2);" ~
        "f64vec3 inversesqrt(f64vec3);" ~
        "f64vec4 inversesqrt(f64vec4);" ~

        "float64_t abs(float64_t);" ~
        "f64vec2 abs(f64vec2);" ~
        "f64vec3 abs(f64vec3);" ~
        "f64vec4 abs(f64vec4);" ~

        "float64_t sign(float64_t);" ~
        "f64vec2 sign(f64vec2);" ~
        "f64vec3 sign(f64vec3);" ~
        "f64vec4 sign(f64vec4);" ~

        "float64_t floor(float64_t);" ~
        "f64vec2 floor(f64vec2);" ~
        "f64vec3 floor(f64vec3);" ~
        "f64vec4 floor(f64vec4);" ~

        "float64_t trunc(float64_t);" ~
        "f64vec2 trunc(f64vec2);" ~
        "f64vec3 trunc(f64vec3);" ~
        "f64vec4 trunc(f64vec4);" ~

        "float64_t round(float64_t);" ~
        "f64vec2 round(f64vec2);" ~
        "f64vec3 round(f64vec3);" ~
        "f64vec4 round(f64vec4);" ~

        "float64_t roundEven(float64_t);" ~
        "f64vec2 roundEven(f64vec2);" ~
        "f64vec3 roundEven(f64vec3);" ~
        "f64vec4 roundEven(f64vec4);" ~

        "float64_t ceil(float64_t);" ~
        "f64vec2 ceil(f64vec2);" ~
        "f64vec3 ceil(f64vec3);" ~
        "f64vec4 ceil(f64vec4);" ~

        "float64_t fract(float64_t);" ~
        "f64vec2 fract(f64vec2);" ~
        "f64vec3 fract(f64vec3);" ~
        "f64vec4 fract(f64vec4);" ~

        "float64_t mod(float64_t, float64_t);" ~
        "f64vec2 mod(f64vec2, float64_t);" ~
        "f64vec3 mod(f64vec3, float64_t);" ~
        "f64vec4 mod(f64vec4, float64_t);" ~
        "f64vec2 mod(f64vec2, f64vec2);" ~
        "f64vec3 mod(f64vec3, f64vec3);" ~
        "f64vec4 mod(f64vec4, f64vec4);" ~

        "float64_t modf(float64_t, out float64_t);" ~
        "f64vec2 modf(f64vec2, out f64vec2);" ~
        "f64vec3 modf(f64vec3, out f64vec3);" ~
        "f64vec4 modf(f64vec4, out f64vec4);" ~

        "float64_t min(float64_t, float64_t);" ~
        "f64vec2 min(f64vec2, float64_t);" ~
        "f64vec3 min(f64vec3, float64_t);" ~
        "f64vec4 min(f64vec4, float64_t);" ~
        "f64vec2 min(f64vec2, f64vec2);" ~
        "f64vec3 min(f64vec3, f64vec3);" ~
        "f64vec4 min(f64vec4, f64vec4);" ~

        "float64_t max(float64_t, float64_t);" ~
        "f64vec2 max(f64vec2, float64_t);" ~
        "f64vec3 max(f64vec3, float64_t);" ~
        "f64vec4 max(f64vec4, float64_t);" ~
        "f64vec2 max(f64vec2, f64vec2);" ~
        "f64vec3 max(f64vec3, f64vec3);" ~
        "f64vec4 max(f64vec4, f64vec4);" ~

        "float64_t clamp(float64_t, float64_t, float64_t);" ~
        "f64vec2 clamp(f64vec2, float64_t, float64_t);" ~
        "f64vec3 clamp(f64vec3, float64_t, float64_t);" ~
        "f64vec4 clamp(f64vec4, float64_t, float64_t);" ~
        "f64vec2 clamp(f64vec2, f64vec2, f64vec2);" ~
        "f64vec3 clamp(f64vec3, f64vec3, f64vec3);" ~
        "f64vec4 clamp(f64vec4, f64vec4, f64vec4);" ~

        "float64_t mix(float64_t, float64_t, float64_t);" ~
        "f64vec2 mix(f64vec2, f64vec2, float64_t);" ~
        "f64vec3 mix(f64vec3, f64vec3, float64_t);" ~
        "f64vec4 mix(f64vec4, f64vec4, float64_t);" ~
        "f64vec2 mix(f64vec2, f64vec2, f64vec2);" ~
        "f64vec3 mix(f64vec3, f64vec3, f64vec3);" ~
        "f64vec4 mix(f64vec4, f64vec4, f64vec4);" ~
        "float64_t mix(float64_t, float64_t, bool);" ~
        "f64vec2 mix(f64vec2, f64vec2, bvec2);" ~
        "f64vec3 mix(f64vec3, f64vec3, bvec3);" ~
        "f64vec4 mix(f64vec4, f64vec4, bvec4);" ~

        "float64_t step(float64_t, float64_t);" ~
        "f64vec2 step(f64vec2, f64vec2);" ~
        "f64vec3 step(f64vec3, f64vec3);" ~
        "f64vec4 step(f64vec4, f64vec4);" ~
        "f64vec2 step(float64_t, f64vec2);" ~
        "f64vec3 step(float64_t, f64vec3);" ~
        "f64vec4 step(float64_t, f64vec4);" ~

        "float64_t smoothstep(float64_t, float64_t, float64_t);" ~
        "f64vec2 smoothstep(f64vec2, f64vec2, f64vec2);" ~
        "f64vec3 smoothstep(f64vec3, f64vec3, f64vec3);" ~
        "f64vec4 smoothstep(f64vec4, f64vec4, f64vec4);" ~
        "f64vec2 smoothstep(float64_t, float64_t, f64vec2);" ~
        "f64vec3 smoothstep(float64_t, float64_t, f64vec3);" ~
        "f64vec4 smoothstep(float64_t, float64_t, f64vec4);" ~

        "float64_t length(float64_t);" ~
        "float64_t length(f64vec2);" ~
        "float64_t length(f64vec3);" ~
        "float64_t length(f64vec4);" ~

        "float64_t distance(float64_t, float64_t);" ~
        "float64_t distance(f64vec2, f64vec2);" ~
        "float64_t distance(f64vec3, f64vec3);" ~
        "float64_t distance(f64vec4, f64vec4);" ~

        "float64_t dot(float64_t, float64_t);" ~
        "float64_t dot(f64vec2, f64vec2);" ~
        "float64_t dot(f64vec3, f64vec3);" ~
        "float64_t dot(f64vec4, f64vec4);" ~

        "f64vec3 cross(f64vec3, f64vec3);" ~

        "float64_t normalize(float64_t);" ~
        "f64vec2 normalize(f64vec2);" ~
        "f64vec3 normalize(f64vec3);" ~
        "f64vec4 normalize(f64vec4);" ~

        "float64_t faceforward(float64_t, float64_t, float64_t);" ~
        "f64vec2 faceforward(f64vec2, f64vec2, f64vec2);" ~
        "f64vec3 faceforward(f64vec3, f64vec3, f64vec3);" ~
        "f64vec4 faceforward(f64vec4, f64vec4, f64vec4);" ~

        "float64_t reflect(float64_t, float64_t);" ~
        "f64vec2 reflect(f64vec2, f64vec2 );" ~
        "f64vec3 reflect(f64vec3, f64vec3 );" ~
        "f64vec4 reflect(f64vec4, f64vec4 );" ~

        "float64_t refract(float64_t, float64_t, float64_t);" ~
        "f64vec2 refract(f64vec2, f64vec2, float64_t);" ~
        "f64vec3 refract(f64vec3, f64vec3, float64_t);" ~
        "f64vec4 refract(f64vec4, f64vec4, float64_t);" ~

        "f64mat2 matrixCompMult(f64mat2, f64mat2);" ~
        "f64mat3 matrixCompMult(f64mat3, f64mat3);" ~
        "f64mat4 matrixCompMult(f64mat4, f64mat4);" ~
        "f64mat2x3 matrixCompMult(f64mat2x3, f64mat2x3);" ~
        "f64mat2x4 matrixCompMult(f64mat2x4, f64mat2x4);" ~
        "f64mat3x2 matrixCompMult(f64mat3x2, f64mat3x2);" ~
        "f64mat3x4 matrixCompMult(f64mat3x4, f64mat3x4);" ~
        "f64mat4x2 matrixCompMult(f64mat4x2, f64mat4x2);" ~
        "f64mat4x3 matrixCompMult(f64mat4x3, f64mat4x3);" ~

        "f64mat2 outerProduct(f64vec2, f64vec2);" ~
        "f64mat3 outerProduct(f64vec3, f64vec3);" ~
        "f64mat4 outerProduct(f64vec4, f64vec4);" ~
        "f64mat2x3 outerProduct(f64vec3, f64vec2);" ~
        "f64mat3x2 outerProduct(f64vec2, f64vec3);" ~
        "f64mat2x4 outerProduct(f64vec4, f64vec2);" ~
        "f64mat4x2 outerProduct(f64vec2, f64vec4);" ~
        "f64mat3x4 outerProduct(f64vec4, f64vec3);" ~
        "f64mat4x3 outerProduct(f64vec3, f64vec4);" ~

        "f64mat2 transpose(f64mat2);" ~
        "f64mat3 transpose(f64mat3);" ~
        "f64mat4 transpose(f64mat4);" ~
        "f64mat2x3 transpose(f64mat3x2);" ~
        "f64mat3x2 transpose(f64mat2x3);" ~
        "f64mat2x4 transpose(f64mat4x2);" ~
        "f64mat4x2 transpose(f64mat2x4);" ~
        "f64mat3x4 transpose(f64mat4x3);" ~
        "f64mat4x3 transpose(f64mat3x4);" ~

        "float64_t determinant(f64mat2);" ~
        "float64_t determinant(f64mat3);" ~
        "float64_t determinant(f64mat4);" ~

        "f64mat2 inverse(f64mat2);" ~
        "f64mat3 inverse(f64mat3);" ~
        "f64mat4 inverse(f64mat4);" ~

        "\n"
      );
    }

    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 450) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 310)) {
      commonBuiltins ~= (
        "int64_t abs(int64_t);" ~
        "i64vec2 abs(i64vec2);" ~
        "i64vec3 abs(i64vec3);" ~
        "i64vec4 abs(i64vec4);" ~

        "int64_t sign(int64_t);" ~
        "i64vec2 sign(i64vec2);" ~
        "i64vec3 sign(i64vec3);" ~
        "i64vec4 sign(i64vec4);" ~

        "int64_t min(int64_t, int64_t);" ~
        "i64vec2 min(i64vec2, int64_t);" ~
        "i64vec3 min(i64vec3, int64_t);" ~
        "i64vec4 min(i64vec4, int64_t);" ~
        "i64vec2 min(i64vec2, i64vec2);" ~
        "i64vec3 min(i64vec3, i64vec3);" ~
        "i64vec4 min(i64vec4, i64vec4);" ~
        "uint64_t min(uint64_t, uint64_t);" ~
        "u64vec2 min(u64vec2, uint64_t);" ~
        "u64vec3 min(u64vec3, uint64_t);" ~
        "u64vec4 min(u64vec4, uint64_t);" ~
        "u64vec2 min(u64vec2, u64vec2);" ~
        "u64vec3 min(u64vec3, u64vec3);" ~
        "u64vec4 min(u64vec4, u64vec4);" ~

        "int64_t max(int64_t, int64_t);" ~
        "i64vec2 max(i64vec2, int64_t);" ~
        "i64vec3 max(i64vec3, int64_t);" ~
        "i64vec4 max(i64vec4, int64_t);" ~
        "i64vec2 max(i64vec2, i64vec2);" ~
        "i64vec3 max(i64vec3, i64vec3);" ~
        "i64vec4 max(i64vec4, i64vec4);" ~
        "uint64_t max(uint64_t, uint64_t);" ~
        "u64vec2 max(u64vec2, uint64_t);" ~
        "u64vec3 max(u64vec3, uint64_t);" ~
        "u64vec4 max(u64vec4, uint64_t);" ~
        "u64vec2 max(u64vec2, u64vec2);" ~
        "u64vec3 max(u64vec3, u64vec3);" ~
        "u64vec4 max(u64vec4, u64vec4);" ~

        "int64_t clamp(int64_t, int64_t, int64_t);" ~
        "i64vec2 clamp(i64vec2, int64_t, int64_t);" ~
        "i64vec3 clamp(i64vec3, int64_t, int64_t);" ~
        "i64vec4 clamp(i64vec4, int64_t, int64_t);" ~
        "i64vec2 clamp(i64vec2, i64vec2, i64vec2);" ~
        "i64vec3 clamp(i64vec3, i64vec3, i64vec3);" ~
        "i64vec4 clamp(i64vec4, i64vec4, i64vec4);" ~
        "uint64_t clamp(uint64_t, uint64_t, uint64_t);" ~
        "u64vec2 clamp(u64vec2, uint64_t, uint64_t);" ~
        "u64vec3 clamp(u64vec3, uint64_t, uint64_t);" ~
        "u64vec4 clamp(u64vec4, uint64_t, uint64_t);" ~
        "u64vec2 clamp(u64vec2, u64vec2, u64vec2);" ~
        "u64vec3 clamp(u64vec3, u64vec3, u64vec3);" ~
        "u64vec4 clamp(u64vec4, u64vec4, u64vec4);" ~

        "int64_t mix(int64_t, int64_t, bool);" ~
        "i64vec2 mix(i64vec2, i64vec2, bvec2);" ~
        "i64vec3 mix(i64vec3, i64vec3, bvec3);" ~
        "i64vec4 mix(i64vec4, i64vec4, bvec4);" ~
        "uint64_t mix(uint64_t, uint64_t, bool);" ~
        "u64vec2 mix(u64vec2, u64vec2, bvec2);" ~
        "u64vec3 mix(u64vec3, u64vec3, bvec3);" ~
        "u64vec4 mix(u64vec4, u64vec4, bvec4);" ~

        "int64_t doubleBitsToInt64(float64_t);" ~
        "i64vec2 doubleBitsToInt64(f64vec2);" ~
        "i64vec3 doubleBitsToInt64(f64vec3);" ~
        "i64vec4 doubleBitsToInt64(f64vec4);" ~

        "uint64_t doubleBitsToUint64(float64_t);" ~
        "u64vec2 doubleBitsToUint64(f64vec2);" ~
        "u64vec3 doubleBitsToUint64(f64vec3);" ~
        "u64vec4 doubleBitsToUint64(f64vec4);" ~

        "float64_t int64BitsToDouble(int64_t);" ~
        "f64vec2 int64BitsToDouble(i64vec2);" ~
        "f64vec3 int64BitsToDouble(i64vec3);" ~
        "f64vec4 int64BitsToDouble(i64vec4);" ~

        "float64_t uint64BitsToDouble(uint64_t);" ~
        "f64vec2 uint64BitsToDouble(u64vec2);" ~
        "f64vec3 uint64BitsToDouble(u64vec3);" ~
        "f64vec4 uint64BitsToDouble(u64vec4);" ~

        "int64_t packInt2x32(ivec2);" ~
        "uint64_t packUint2x32(uvec2);" ~
        "ivec2 unpackInt2x32(int64_t);" ~
        "uvec2 unpackUint2x32(uint64_t);" ~

        "bvec2 lessThan(i64vec2, i64vec2);" ~
        "bvec3 lessThan(i64vec3, i64vec3);" ~
        "bvec4 lessThan(i64vec4, i64vec4);" ~
        "bvec2 lessThan(u64vec2, u64vec2);" ~
        "bvec3 lessThan(u64vec3, u64vec3);" ~
        "bvec4 lessThan(u64vec4, u64vec4);" ~

        "bvec2 lessThanEqual(i64vec2, i64vec2);" ~
        "bvec3 lessThanEqual(i64vec3, i64vec3);" ~
        "bvec4 lessThanEqual(i64vec4, i64vec4);" ~
        "bvec2 lessThanEqual(u64vec2, u64vec2);" ~
        "bvec3 lessThanEqual(u64vec3, u64vec3);" ~
        "bvec4 lessThanEqual(u64vec4, u64vec4);" ~

        "bvec2 greaterThan(i64vec2, i64vec2);" ~
        "bvec3 greaterThan(i64vec3, i64vec3);" ~
        "bvec4 greaterThan(i64vec4, i64vec4);" ~
        "bvec2 greaterThan(u64vec2, u64vec2);" ~
        "bvec3 greaterThan(u64vec3, u64vec3);" ~
        "bvec4 greaterThan(u64vec4, u64vec4);" ~

        "bvec2 greaterThanEqual(i64vec2, i64vec2);" ~
        "bvec3 greaterThanEqual(i64vec3, i64vec3);" ~
        "bvec4 greaterThanEqual(i64vec4, i64vec4);" ~
        "bvec2 greaterThanEqual(u64vec2, u64vec2);" ~
        "bvec3 greaterThanEqual(u64vec3, u64vec3);" ~
        "bvec4 greaterThanEqual(u64vec4, u64vec4);" ~

        "bvec2 equal(i64vec2, i64vec2);" ~
        "bvec3 equal(i64vec3, i64vec3);" ~
        "bvec4 equal(i64vec4, i64vec4);" ~
        "bvec2 equal(u64vec2, u64vec2);" ~
        "bvec3 equal(u64vec3, u64vec3);" ~
        "bvec4 equal(u64vec4, u64vec4);" ~

        "bvec2 notEqual(i64vec2, i64vec2);" ~
        "bvec3 notEqual(i64vec3, i64vec3);" ~
        "bvec4 notEqual(i64vec4, i64vec4);" ~
        "bvec2 notEqual(u64vec2, u64vec2);" ~
        "bvec3 notEqual(u64vec3, u64vec3);" ~
        "bvec4 notEqual(u64vec4, u64vec4);" ~

        "int64_t bitCount(int64_t);" ~
        "i64vec2 bitCount(i64vec2);" ~
        "i64vec3 bitCount(i64vec3);" ~
        "i64vec4 bitCount(i64vec4);" ~

        "int64_t bitCount(uint64_t);" ~
        "i64vec2 bitCount(u64vec2);" ~
        "i64vec3 bitCount(u64vec3);" ~
        "i64vec4 bitCount(u64vec4);" ~

        "int64_t findLSB(int64_t);" ~
        "i64vec2 findLSB(i64vec2);" ~
        "i64vec3 findLSB(i64vec3);" ~
        "i64vec4 findLSB(i64vec4);" ~

        "int64_t findLSB(uint64_t);" ~
        "i64vec2 findLSB(u64vec2);" ~
        "i64vec3 findLSB(u64vec3);" ~
        "i64vec4 findLSB(u64vec4);" ~

        "int64_t findMSB(int64_t);" ~
        "i64vec2 findMSB(i64vec2);" ~
        "i64vec3 findMSB(i64vec3);" ~
        "i64vec4 findMSB(i64vec4);" ~

        "int64_t findMSB(uint64_t);" ~
        "i64vec2 findMSB(u64vec2);" ~
        "i64vec3 findMSB(u64vec3);" ~
        "i64vec4 findMSB(u64vec4);" ~

        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 430) {
      commonBuiltins ~= (
        "float min3(float, float, float);" ~
        "vec2 min3(vec2, vec2, vec2);" ~
        "vec3 min3(vec3, vec3, vec3);" ~
        "vec4 min3(vec4, vec4, vec4);" ~

        "int min3(int, int, int);" ~
        "ivec2 min3(ivec2, ivec2, ivec2);" ~
        "ivec3 min3(ivec3, ivec3, ivec3);" ~
        "ivec4 min3(ivec4, ivec4, ivec4);" ~

        "uint min3(uint, uint, uint);" ~
        "uvec2 min3(uvec2, uvec2, uvec2);" ~
        "uvec3 min3(uvec3, uvec3, uvec3);" ~
        "uvec4 min3(uvec4, uvec4, uvec4);" ~

        "float max3(float, float, float);" ~
        "vec2 max3(vec2, vec2, vec2);" ~
        "vec3 max3(vec3, vec3, vec3);" ~
        "vec4 max3(vec4, vec4, vec4);" ~

        "int max3(int, int, int);" ~
        "ivec2 max3(ivec2, ivec2, ivec2);" ~
        "ivec3 max3(ivec3, ivec3, ivec3);" ~
        "ivec4 max3(ivec4, ivec4, ivec4);" ~

        "uint max3(uint, uint, uint);" ~
        "uvec2 max3(uvec2, uvec2, uvec2);" ~
        "uvec3 max3(uvec3, uvec3, uvec3);" ~
        "uvec4 max3(uvec4, uvec4, uvec4);" ~

        "float mid3(float, float, float);" ~
        "vec2 mid3(vec2, vec2, vec2);" ~
        "vec3 mid3(vec3, vec3, vec3);" ~
        "vec4 mid3(vec4, vec4, vec4);" ~

        "int mid3(int, int, int);" ~
        "ivec2 mid3(ivec2, ivec2, ivec2);" ~
        "ivec3 mid3(ivec3, ivec3, ivec3);" ~
        "ivec4 mid3(ivec4, ivec4, ivec4);" ~

        "uint mid3(uint, uint, uint);" ~
        "uvec2 mid3(uvec2, uvec2, uvec2);" ~
        "uvec3 mid3(uvec3, uvec3, uvec3);" ~
        "uvec4 mid3(uvec4, uvec4, uvec4);" ~

        "float16_t min3(float16_t, float16_t, float16_t);" ~
        "f16vec2 min3(f16vec2, f16vec2, f16vec2);" ~
        "f16vec3 min3(f16vec3, f16vec3, f16vec3);" ~
        "f16vec4 min3(f16vec4, f16vec4, f16vec4);" ~

        "float16_t max3(float16_t, float16_t, float16_t);" ~
        "f16vec2 max3(f16vec2, f16vec2, f16vec2);" ~
        "f16vec3 max3(f16vec3, f16vec3, f16vec3);" ~
        "f16vec4 max3(f16vec4, f16vec4, f16vec4);" ~

        "float16_t mid3(float16_t, float16_t, float16_t);" ~
        "f16vec2 mid3(f16vec2, f16vec2, f16vec2);" ~
        "f16vec3 mid3(f16vec3, f16vec3, f16vec3);" ~
        "f16vec4 mid3(f16vec4, f16vec4, f16vec4);" ~

        "int16_t min3(int16_t, int16_t, int16_t);" ~
        "i16vec2 min3(i16vec2, i16vec2, i16vec2);" ~
        "i16vec3 min3(i16vec3, i16vec3, i16vec3);" ~
        "i16vec4 min3(i16vec4, i16vec4, i16vec4);" ~

        "int16_t max3(int16_t, int16_t, int16_t);" ~
        "i16vec2 max3(i16vec2, i16vec2, i16vec2);" ~
        "i16vec3 max3(i16vec3, i16vec3, i16vec3);" ~
        "i16vec4 max3(i16vec4, i16vec4, i16vec4);" ~

        "int16_t mid3(int16_t, int16_t, int16_t);" ~
        "i16vec2 mid3(i16vec2, i16vec2, i16vec2);" ~
        "i16vec3 mid3(i16vec3, i16vec3, i16vec3);" ~
        "i16vec4 mid3(i16vec4, i16vec4, i16vec4);" ~

        "uint16_t min3(uint16_t, uint16_t, uint16_t);" ~
        "u16vec2 min3(u16vec2, u16vec2, u16vec2);" ~
        "u16vec3 min3(u16vec3, u16vec3, u16vec3);" ~
        "u16vec4 min3(u16vec4, u16vec4, u16vec4);" ~

        "uint16_t max3(uint16_t, uint16_t, uint16_t);" ~
        "u16vec2 max3(u16vec2, u16vec2, u16vec2);" ~
        "u16vec3 max3(u16vec3, u16vec3, u16vec3);" ~
        "u16vec4 max3(u16vec4, u16vec4, u16vec4);" ~

        "uint16_t mid3(uint16_t, uint16_t, uint16_t);" ~
        "u16vec2 mid3(u16vec2, u16vec2, u16vec2);" ~
        "u16vec3 mid3(u16vec3, u16vec3, u16vec3);" ~
        "u16vec4 mid3(u16vec4, u16vec4, u16vec4);" ~

        "\n"
      );
    }

    if ((profile == EProfile(ES_PROFILE: 1) && version_ >= 310) ||
      (profile != EProfile(ES_PROFILE: 1) && version_ >= 430)) {
      commonBuiltins ~= (
        "uint atomicAdd(coherent volatile nontemporal inout uint, uint, int, int, int);" ~
        "int atomicAdd(coherent volatile nontemporal inout int, int, int, int, int);" ~

        "uint atomicMin(coherent volatile nontemporal inout uint, uint, int, int, int);" ~
        "int atomicMin(coherent volatile nontemporal inout int, int, int, int, int);" ~

        "uint atomicMax(coherent volatile nontemporal inout uint, uint, int, int, int);" ~
        "int atomicMax(coherent volatile nontemporal inout int, int, int, int, int);" ~

        "uint atomicAnd(coherent volatile nontemporal inout uint, uint, int, int, int);" ~
        "int atomicAnd(coherent volatile nontemporal inout int, int, int, int, int);" ~

        "uint atomicOr(coherent volatile nontemporal inout uint, uint, int, int, int);" ~
        "int atomicOr(coherent volatile nontemporal inout int, int, int, int, int);" ~

        "uint atomicXor(coherent volatile nontemporal inout uint, uint, int, int, int);" ~
        "int atomicXor(coherent volatile nontemporal inout int, int, int, int, int);" ~

        "uint atomicExchange(coherent volatile nontemporal inout uint, uint, int, int, int);" ~
        "int atomicExchange(coherent volatile nontemporal inout int, int, int, int, int);" ~

        "uint atomicCompSwap(coherent volatile nontemporal inout uint, uint, uint, int, int, int, int, int);" ~
        "int atomicCompSwap(coherent volatile nontemporal inout int, int, int, int, int, int, int, int);" ~

        "uint atomicLoad(coherent volatile nontemporal in uint, int, int, int);" ~
        "int atomicLoad(coherent volatile nontemporal in int, int, int, int);" ~

        "void atomicStore(coherent volatile nontemporal out uint, uint, int, int, int);" ~
        "void atomicStore(coherent volatile nontemporal out int, int, int, int, int);" ~
        
        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 440) {
      commonBuiltins ~= (
        "uint64_t atomicMin(coherent volatile nontemporal inout uint64_t, uint64_t);" ~
        "int64_t atomicMin(coherent volatile nontemporal inout int64_t, int64_t);" ~
        "uint64_t atomicMin(coherent volatile nontemporal inout uint64_t, uint64_t, int, int, int);" ~
        "int64_t atomicMin(coherent volatile nontemporal inout int64_t, int64_t, int, int, int);" ~
        "float16_t atomicMin(coherent volatile nontemporal inout float16_t, float16_t);" ~
        "float16_t atomicMin(coherent volatile nontemporal inout float16_t, float16_t, int, int, int);" ~
        "float atomicMin(coherent volatile nontemporal inout float, float);" ~
        "float atomicMin(coherent volatile nontemporal inout float, float, int, int, int);" ~
        "double atomicMin(coherent volatile nontemporal inout double, double);" ~
        "double atomicMin(coherent volatile nontemporal inout double, double, int, int, int);" ~

        "uint64_t atomicMax(coherent volatile nontemporal inout uint64_t, uint64_t);" ~
        "int64_t atomicMax(coherent volatile nontemporal inout int64_t, int64_t);" ~
        "uint64_t atomicMax(coherent volatile nontemporal inout uint64_t, uint64_t, int, int, int);" ~
        "int64_t atomicMax(coherent volatile nontemporal inout int64_t, int64_t, int, int, int);" ~
        "float16_t atomicMax(coherent volatile nontemporal inout float16_t, float16_t);" ~
        "float16_t atomicMax(coherent volatile nontemporal inout float16_t, float16_t, int, int, int);" ~
        "float atomicMax(coherent volatile nontemporal inout float, float);" ~
        "float atomicMax(coherent volatile nontemporal inout float, float, int, int, int);" ~
        "double atomicMax(coherent volatile nontemporal inout double, double);" ~
        "double atomicMax(coherent volatile nontemporal inout double, double, int, int, int);" ~

        "uint64_t atomicAnd(coherent volatile nontemporal inout uint64_t, uint64_t);" ~
        "int64_t atomicAnd(coherent volatile nontemporal inout int64_t, int64_t);" ~
        "uint64_t atomicAnd(coherent volatile nontemporal inout uint64_t, uint64_t, int, int, int);" ~
        "int64_t atomicAnd(coherent volatile nontemporal inout int64_t, int64_t, int, int, int);" ~

        "uint64_t atomicOr (coherent volatile nontemporal inout uint64_t, uint64_t);" ~
        "int64_t atomicOr (coherent volatile nontemporal inout int64_t, int64_t);" ~
        "uint64_t atomicOr (coherent volatile nontemporal inout uint64_t, uint64_t, int, int, int);" ~
        "int64_t atomicOr (coherent volatile nontemporal inout int64_t, int64_t, int, int, int);" ~

        "uint64_t atomicXor(coherent volatile nontemporal inout uint64_t, uint64_t);" ~
        "int64_t atomicXor(coherent volatile nontemporal inout int64_t, int64_t);" ~
        "uint64_t atomicXor(coherent volatile nontemporal inout uint64_t, uint64_t, int, int, int);" ~
        "int64_t atomicXor(coherent volatile nontemporal inout int64_t, int64_t, int, int, int);" ~

        "uint64_t atomicAdd(coherent volatile nontemporal inout uint64_t, uint64_t);" ~
        "int64_t atomicAdd(coherent volatile nontemporal inout int64_t, int64_t);" ~
        "uint64_t atomicAdd(coherent volatile nontemporal inout uint64_t, uint64_t, int, int, int);" ~
        "int64_t atomicAdd(coherent volatile nontemporal inout int64_t, int64_t, int, int, int);" ~
        "float16_t atomicAdd(coherent volatile nontemporal inout float16_t, float16_t);" ~
        "float16_t atomicAdd(coherent volatile nontemporal inout float16_t, float16_t, int, int, int);" ~
        "float atomicAdd(coherent volatile nontemporal inout float, float);" ~
        "float atomicAdd(coherent volatile nontemporal inout float, float, int, int, int);" ~
        "double atomicAdd(coherent volatile nontemporal inout double, double);" ~
        "double atomicAdd(coherent volatile nontemporal inout double, double, int, int, int);" ~

        "uint64_t atomicExchange(coherent volatile nontemporal inout uint64_t, uint64_t);" ~
        "int64_t atomicExchange(coherent volatile nontemporal inout int64_t, int64_t);" ~
        "uint64_t atomicExchange(coherent volatile nontemporal inout uint64_t, uint64_t, int, int, int);" ~
        "int64_t atomicExchange(coherent volatile nontemporal inout int64_t, int64_t, int, int, int);" ~
        "float16_t atomicExchange(coherent volatile nontemporal inout float16_t, float16_t);" ~
        "float16_t atomicExchange(coherent volatile nontemporal inout float16_t, float16_t, int, int, int);" ~
        "float atomicExchange(coherent volatile nontemporal inout float, float);" ~
        "float atomicExchange(coherent volatile nontemporal inout float, float, int, int, int);" ~
        "double atomicExchange(coherent volatile nontemporal inout double, double);" ~
        "double atomicExchange(coherent volatile nontemporal inout double, double, int, int, int);" ~

        "uint64_t atomicCompSwap(coherent volatile nontemporal inout uint64_t, uint64_t, uint64_t);" ~
        "int64_t atomicCompSwap(coherent volatile nontemporal inout int64_t, int64_t, int64_t);" ~
        "uint64_t atomicCompSwap(coherent volatile nontemporal inout uint64_t, uint64_t, uint64_t, int, int, int, int, int);" ~
        "int64_t atomicCompSwap(coherent volatile nontemporal inout int64_t, int64_t, int64_t, int, int, int, int, int);" ~

        "uint64_t atomicLoad(coherent volatile nontemporal in uint64_t, int, int, int);" ~
        "int64_t atomicLoad(coherent volatile nontemporal in int64_t, int, int, int);" ~
        "float16_t atomicLoad(coherent volatile nontemporal in float16_t, int, int, int);" ~
        "float atomicLoad(coherent volatile nontemporal in float, int, int, int);" ~
        "double atomicLoad(coherent volatile nontemporal in double, int, int, int);" ~

        "void atomicStore(coherent volatile nontemporal out uint64_t, uint64_t, int, int, int);" ~
        "void atomicStore(coherent volatile nontemporal out int64_t, int64_t, int, int, int);" ~
        "void atomicStore(coherent volatile nontemporal out float16_t, float16_t, int, int, int);" ~
        "void atomicStore(coherent volatile nontemporal out float, float, int, int, int);" ~
        "void atomicStore(coherent volatile nontemporal out double, double, int, int, int);" ~

        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 430) {
      commonBuiltins ~= (
        "f16vec2 atomicAdd(coherent volatile nontemporal inout f16vec2, f16vec2);" ~
        "f16vec4 atomicAdd(coherent volatile nontemporal inout f16vec4, f16vec4);" ~
        "f16vec2 atomicMin(coherent volatile nontemporal inout f16vec2, f16vec2);" ~
        "f16vec4 atomicMin(coherent volatile nontemporal inout f16vec4, f16vec4);" ~
        "f16vec2 atomicMax(coherent volatile nontemporal inout f16vec2, f16vec2);" ~
        "f16vec4 atomicMax(coherent volatile nontemporal inout f16vec4, f16vec4);" ~
        "f16vec2 atomicExchange(coherent volatile nontemporal inout f16vec2, f16vec2);" ~
        "f16vec4 atomicExchange(coherent volatile nontemporal inout f16vec4, f16vec4);" ~
        "\n"
      );
    }

    if ((profile == EProfile(ES_PROFILE: 1) && version_ >= 300) ||
      (profile != EProfile(ES_PROFILE: 1) && version_ >= 150)) {
      commonBuiltins ~= (
        "int floatBitsToInt(highp float value);" ~
        "ivec2 floatBitsToInt(highp vec2 value);" ~
        "ivec3 floatBitsToInt(highp vec3 value);" ~
        "ivec4 floatBitsToInt(highp vec4 value);" ~

        "uint floatBitsToUint(highp float value);" ~
        "uvec2 floatBitsToUint(highp vec2 value);" ~
        "uvec3 floatBitsToUint(highp vec3 value);" ~
        "uvec4 floatBitsToUint(highp vec4 value);" ~

        "float intBitsToFloat(highp int value);" ~
        "vec2 intBitsToFloat(highp ivec2 value);" ~
        "vec3 intBitsToFloat(highp ivec3 value);" ~
        "vec4 intBitsToFloat(highp ivec4 value);" ~

        "float uintBitsToFloat(highp uint value);" ~
        "vec2 uintBitsToFloat(highp uvec2 value);" ~
        "vec3 uintBitsToFloat(highp uvec3 value);" ~
        "vec4 uintBitsToFloat(highp uvec4 value);" ~

        "\n"
      );
    }

    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 150) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 310)) {
      commonBuiltins ~= (
        "float fma(float, float, float);" ~
        "vec2 fma(vec2, vec2, vec2);" ~
        "vec3 fma(vec3, vec3, vec3);" ~
        "vec4 fma(vec4, vec4, vec4);" ~
        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 150) {
      commonBuiltins ~= (
        "double fma(double, double, double);" ~
        "dvec2 fma(dvec2, dvec2, dvec2);" ~
        "dvec3 fma(dvec3, dvec3, dvec3);" ~
        "dvec4 fma(dvec4, dvec4, dvec4);" ~
        "\n"
      );
    }

    if (profile == EProfile(ES_PROFILE: 1) && version_ >= 310) {
      commonBuiltins ~= (
        "float64_t fma(float64_t, float64_t, float64_t);" ~
        "f64vec2 fma(f64vec2, f64vec2, f64vec2);" ~
        "f64vec3 fma(f64vec3, f64vec3, f64vec3);" ~
        "f64vec4 fma(f64vec4, f64vec4, f64vec4);" ~
        "\n"
      );
    }

    if ((profile == EProfile(ES_PROFILE: 1) && version_ >= 310) ||
      (profile != EProfile(ES_PROFILE: 1) && version_ >= 150)) {
      commonBuiltins ~= (
        "float frexp(highp float, out highp int);" ~
        "vec2 frexp(highp vec2, out highp ivec2);" ~
        "vec3 frexp(highp vec3, out highp ivec3);" ~
        "vec4 frexp(highp vec4, out highp ivec4);" ~

        "float ldexp(highp float, highp int);" ~
        "vec2 ldexp(highp vec2, highp ivec2);" ~
        "vec3 ldexp(highp vec3, highp ivec3);" ~
        "vec4 ldexp(highp vec4, highp ivec4);" ~

        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 150) {
      commonBuiltins ~= (
        "double frexp(double, out int);" ~
        "dvec2 frexp(dvec2, out ivec2);" ~
        "dvec3 frexp(dvec3, out ivec3);" ~
        "dvec4 frexp(dvec4, out ivec4);" ~

        "double ldexp(double, int);" ~
        "dvec2 ldexp(dvec2, ivec2);" ~
        "dvec3 ldexp(dvec3, ivec3);" ~
        "dvec4 ldexp(dvec4, ivec4);" ~

        "double packDouble2x32(uvec2);" ~
        "uvec2 unpackDouble2x32(double);" ~

        "\n"
      );
    }

    if (profile == EProfile(ES_PROFILE: 1) && version_ >= 310) {
      commonBuiltins ~= (
        "float64_t frexp(float64_t, out int);" ~
        "f64vec2 frexp(f64vec2, out ivec2);" ~
        "f64vec3 frexp(f64vec3, out ivec3);" ~
        "f64vec4 frexp(f64vec4, out ivec4);" ~

        "float64_t ldexp(float64_t, int);" ~
        "f64vec2 ldexp(f64vec2, ivec2);" ~
        "f64vec3 ldexp(f64vec3, ivec3);" ~
        "f64vec4 ldexp(f64vec4, ivec4);" ~

        "\n"
      );
    }

    if ((profile == EProfile(ES_PROFILE: 1) && version_ >= 300) ||
      (profile != EProfile(ES_PROFILE: 1) && version_ >= 150)) {
      commonBuiltins ~= (
        "highp uint packUnorm2x16(vec2);" ~
        "vec2 unpackUnorm2x16(highp uint);" ~
        "\n"
      );
    }

    if ((profile == EProfile(ES_PROFILE: 1) && version_ >= 300) ||
      (profile != EProfile(ES_PROFILE: 1) && version_ >= 150)) {
      commonBuiltins ~= (
        "highp uint packSnorm2x16(vec2);" ~
        "vec2 unpackSnorm2x16(highp uint);" ~
        "highp uint packHalf2x16(vec2);" ~
        "\n"
      );
    }

    if (profile == EProfile(ES_PROFILE: 1) && version_ >= 300) {
      commonBuiltins ~= (
        "mediump vec2 unpackHalf2x16(highp uint);" ~
        "\n"
      );
    } else if (profile != EProfile(ES_PROFILE: 1) && version_ >= 150) {
      commonBuiltins ~= (
        "vec2 unpackHalf2x16(highp uint);" ~
        "\n"
      );
    }

    if ((profile == EProfile(ES_PROFILE: 1) && version_ >= 310) ||
      (profile != EProfile(ES_PROFILE: 1) && version_ >= 150)) {
      commonBuiltins ~= (
        "highp uint packSnorm4x8(vec4);" ~
        "highp uint packUnorm4x8(vec4);" ~
        "\n"
      );
    }

    if (profile == EProfile(ES_PROFILE: 1) && version_ >= 310) {
      commonBuiltins ~= (
        "mediump vec4 unpackSnorm4x8(highp uint);" ~
        "mediump vec4 unpackUnorm4x8(highp uint);" ~
        "\n"
      );
    } else if (profile != EProfile(ES_PROFILE: 1) && version_ >= 150) {
      commonBuiltins ~= (
        "vec4 unpackSnorm4x8(highp uint);" ~
        "vec4 unpackUnorm4x8(highp uint);" ~
        "\n"
      );
    }

    commonBuiltins ~= (
      "mat2 matrixCompMult(mat2 x, mat2 y);" ~
      "mat3 matrixCompMult(mat3 x, mat3 y);" ~
      "mat4 matrixCompMult(mat4 x, mat4 y);" ~

      "\n"
    );

    if (version_ >= 120) {
      commonBuiltins ~= (
        "mat2 outerProduct(vec2 c, vec2 r);" ~
        "mat3 outerProduct(vec3 c, vec3 r);" ~
        "mat4 outerProduct(vec4 c, vec4 r);" ~
        "mat2x3 outerProduct(vec3 c, vec2 r);" ~
        "mat3x2 outerProduct(vec2 c, vec3 r);" ~
        "mat2x4 outerProduct(vec4 c, vec2 r);" ~
        "mat4x2 outerProduct(vec2 c, vec4 r);" ~
        "mat3x4 outerProduct(vec4 c, vec3 r);" ~
        "mat4x3 outerProduct(vec3 c, vec4 r);" ~

        "mat2 transpose(mat2 m);" ~
        "mat3 transpose(mat3 m);" ~
        "mat4 transpose(mat4 m);" ~
        "mat2x3 transpose(mat3x2 m);" ~
        "mat3x2 transpose(mat2x3 m);" ~
        "mat2x4 transpose(mat4x2 m);" ~
        "mat4x2 transpose(mat2x4 m);" ~
        "mat3x4 transpose(mat4x3 m);" ~
        "mat4x3 transpose(mat3x4 m);" ~

        "mat2x3 matrixCompMult(mat2x3, mat2x3);" ~
        "mat2x4 matrixCompMult(mat2x4, mat2x4);" ~
        "mat3x2 matrixCompMult(mat3x2, mat3x2);" ~
        "mat3x4 matrixCompMult(mat3x4, mat3x4);" ~
        "mat4x2 matrixCompMult(mat4x2, mat4x2);" ~
        "mat4x3 matrixCompMult(mat4x3, mat4x3);" ~

        "\n"
      );

      if (version_ >= 150) {
        commonBuiltins ~= (
          "float determinant(mat2 m);" ~
          "float determinant(mat3 m);" ~
          "float determinant(mat4 m);" ~

          "mat2 inverse(mat2 m);" ~
          "mat3 inverse(mat3 m);" ~
          "mat4 inverse(mat4 m);" ~

          "\n"
        );
      }
    }

    if ((profile == EProfile(ES_PROFILE: 1) && version_ == 100) ||
      (profile == EProfile(CORE_PROFILE: 1) && version_ < 420) ||
      profile == EProfile(COMPATIBILITY_PROFILE: 1) ||
      profile == EProfile(NO_PROFILE: 1)) {
      if (spvVersion.spv == 0) {
        commonBuiltins ~= (
          "vec4 texture2D(sampler2D, vec2);" ~

          "vec4 texture2DProj(sampler2D, vec3);" ~
          "vec4 texture2DProj(sampler2D, vec4);" ~

          "vec4 texture3D(sampler3D, vec3);" ~
          "vec4 texture3DProj(sampler3D, vec4);" ~

          "vec4 textureCube(samplerCube, vec3);" ~

          "\n"
        );
      }
    }

    if ((profile == EProfile(CORE_PROFILE: 1) && version_ < 420) ||
      profile == EProfile(COMPATIBILITY_PROFILE: 1) ||
      profile == EProfile(NO_PROFILE: 1)) {
      if (spvVersion.spv == 0) {
        commonBuiltins ~= (
          "vec4 texture1D(sampler1D, float);" ~

          "vec4 texture1DProj(sampler1D, vec2);" ~
          "vec4 texture1DProj(sampler1D, vec4);" ~

          "vec4 shadow1D(sampler1DShadow, vec3);" ~
          "vec4 shadow2D(sampler2DShadow, vec3);" ~
          "vec4 shadow1DProj(sampler1DShadow, vec4);" ~
          "vec4 shadow2DProj(sampler2DShadow, vec4);" ~

          "vec4 texture2DRect(sampler2DRect, vec2);" ~
          "vec4 texture2DRectProj(sampler2DRect, vec3);" ~
          "vec4 texture2DRectProj(sampler2DRect, vec4);" ~
          "vec4 shadow2DRect(sampler2DRectShadow, vec3);" ~
          "vec4 shadow2DRectProj(sampler2DRectShadow, vec4);" ~

          "vec4 texture1DArray(sampler1DArray, vec2);" ~
          "vec4 texture2DArray(sampler2DArray, vec3);" ~
          "vec4 shadow1DArray(sampler1DArrayShadow, vec3);" ~
          "vec4 shadow2DArray(sampler2DArrayShadow, vec4);" ~
          "vec4 texture1DArray(sampler1DArray, vec2, float);" ~
          "vec4 texture2DArray(sampler2DArray, vec3, float);" ~
          "vec4 shadow1DArray(sampler1DArrayShadow, vec3, float);" ~
          "vec4 texture1DArrayLod(sampler1DArray, vec2, float);" ~
          "vec4 texture2DArrayLod(sampler2DArray, vec3, float);" ~
          "vec4 shadow1DArrayLod(sampler1DArrayShadow, vec3, float);" ~
          "\n"
        );
      }
    }

    if (profile == EProfile(ES_PROFILE: 1)) {
      if (spvVersion.spv == 0) {
        if (version_ < 300) {
          commonBuiltins ~= (
            "vec4 texture2D(samplerExternalOES, vec2 coord);" ~
            "vec4 texture2DProj(samplerExternalOES, vec3);" ~
            "vec4 texture2DProj(samplerExternalOES, vec4);" ~
            "\n"
          );
        } else {
          commonBuiltins ~= (
            "highp ivec2 textureSize(samplerExternalOES, int lod);" ~
            "vec4 texture(samplerExternalOES, vec2);" ~
            "vec4 texture(samplerExternalOES, vec2, float bias);" ~
            "vec4 textureProj(samplerExternalOES, vec3);" ~
            "vec4 textureProj(samplerExternalOES, vec3, float bias);" ~
            "vec4 textureProj(samplerExternalOES, vec4);" ~
            "vec4 textureProj(samplerExternalOES, vec4, float bias);" ~
            "vec4 texelFetch(samplerExternalOES, ivec2, int lod);" ~
            "\n"
          );
        }
        commonBuiltins ~= (
          "highp ivec2 textureSize(__samplerExternal2DY2YEXT, int lod);" ~
          "vec4 texture(__samplerExternal2DY2YEXT, vec2);" ~
          "vec4 texture(__samplerExternal2DY2YEXT, vec2, float bias);" ~
          "vec4 textureProj(__samplerExternal2DY2YEXT, vec3);" ~
          "vec4 textureProj(__samplerExternal2DY2YEXT, vec3, float bias);" ~
          "vec4 textureProj(__samplerExternal2DY2YEXT, vec4);" ~
          "vec4 textureProj(__samplerExternal2DY2YEXT, vec4, float bias);" ~
          "vec4 texelFetch(__samplerExternal2DY2YEXT sampler, ivec2, int lod);" ~
          "\n"
        );
        commonBuiltins ~= (
          "vec4 texture2DGradEXT(sampler2D, vec2, vec2, vec2);" ~
          "vec4 texture2DProjGradEXT(sampler2D, vec3, vec2, vec2);" ~
          "vec4 texture2DProjGradEXT(sampler2D, vec4, vec2, vec2);" ~
          "vec4 textureCubeGradEXT(samplerCube, vec3, vec3, vec3);" ~

          "float shadow2DEXT(sampler2DShadow, vec3);" ~
          "float shadow2DProjEXT(sampler2DShadow, vec4);" ~

          "\n"
        );
      }
    }

    if (spvVersion.spv == 0 && profile != EProfile(ES_PROFILE: 1)) {
      commonBuiltins ~= (
        "float noise1(float x);" ~
        "float noise1(vec2 x);" ~
        "float noise1(vec3 x);" ~
        "float noise1(vec4 x);" ~

        "vec2 noise2(float x);" ~
        "vec2 noise2(vec2 x);" ~
        "vec2 noise2(vec3 x);" ~
        "vec2 noise2(vec4 x);" ~

        "vec3 noise3(float x);" ~
        "vec3 noise3(vec2 x);" ~
        "vec3 noise3(vec3 x);" ~
        "vec3 noise3(vec4 x);" ~

        "vec4 noise4(float x);" ~
        "vec4 noise4(vec2 x);" ~
        "vec4 noise4(vec3 x);" ~
        "vec4 noise4(vec4 x);" ~

        "\n"
      );
    }

    if (spvVersion.vulkan == 0) {
      if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 300) ||
        (profile == EProfile(ES_PROFILE: 1) && version_ >= 310)) {
        commonBuiltins ~= (
          "uint atomicCounterIncrement(atomic_uint);" ~
          "uint atomicCounterDecrement(atomic_uint);" ~
          "uint atomicCounter(atomic_uint);" ~

          "\n"
        );
      }
      if (profile != EProfile(ES_PROFILE: 1) && version_ == 450) {
        commonBuiltins ~= (
          "uint atomicCounterAddARB(atomic_uint, uint);" ~
          "uint atomicCounterSubtractARB(atomic_uint, uint);" ~
          "uint atomicCounterMinARB(atomic_uint, uint);" ~
          "uint atomicCounterMaxARB(atomic_uint, uint);" ~
          "uint atomicCounterAndARB(atomic_uint, uint);" ~
          "uint atomicCounterOrARB(atomic_uint, uint);" ~
          "uint atomicCounterXorARB(atomic_uint, uint);" ~
          "uint atomicCounterExchangeARB(atomic_uint, uint);" ~
          "uint atomicCounterCompSwapARB(atomic_uint, uint, uint);" ~

          "\n"
        );
      }

      if (profile != EProfile(ES_PROFILE: 1) && version_ >= 460) {
        commonBuiltins ~= (
          "uint atomicCounterAdd(atomic_uint, uint);" ~
          "uint atomicCounterSubtract(atomic_uint, uint);" ~
          "uint atomicCounterMin(atomic_uint, uint);" ~
          "uint atomicCounterMax(atomic_uint, uint);" ~
          "uint atomicCounterAnd(atomic_uint, uint);" ~
          "uint atomicCounterOr(atomic_uint, uint);" ~
          "uint atomicCounterXor(atomic_uint, uint);" ~
          "uint atomicCounterExchange(atomic_uint, uint);" ~
          "uint atomicCounterCompSwap(atomic_uint, uint, uint);" ~

          "\n"
        );
      }
    } else if (spvVersion.vulkanRelaxed) {
      if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 300) ||
        (profile == EProfile(ES_PROFILE: 1) && version_ >= 310)) {
        commonBuiltins ~= (
          "uint atomicCounterIncrement(volatile coherent nontemporal uint);" ~
          "uint atomicCounterDecrement(volatile coherent nontemporal uint);" ~
          "uint atomicCounter(volatile coherent nontemporal uint);" ~

          "\n"
        );
      }
      if (profile != EProfile(ES_PROFILE: 1) && version_ >= 460) {
        commonBuiltins ~= (
          "uint atomicCounterAdd(volatile coherent nontemporal uint, uint);" ~
          "uint atomicCounterSubtract(volatile coherent nontemporal uint, uint);" ~
          "uint atomicCounterMin(volatile coherent nontemporal uint, uint);" ~
          "uint atomicCounterMax(volatile coherent nontemporal uint, uint);" ~
          "uint atomicCounterAnd(volatile coherent nontemporal uint, uint);" ~
          "uint atomicCounterOr(volatile coherent nontemporal uint, uint);" ~
          "uint atomicCounterXor(volatile coherent nontemporal uint, uint);" ~
          "uint atomicCounterExchange(volatile coherent nontemporal uint, uint);" ~
          "uint atomicCounterCompSwap(volatile coherent nontemporal uint, uint, uint);" ~

          "\n"
        );
      }
    }

    if ((profile == EProfile(ES_PROFILE: 1) && version_ >= 310) ||
      (profile != EProfile(ES_PROFILE: 1) && version_ >= 150)) {
      commonBuiltins ~= (
        "int bitfieldExtract(int, int, int);" ~
        "ivec2 bitfieldExtract(ivec2, int, int);" ~
        "ivec3 bitfieldExtract(ivec3, int, int);" ~
        "ivec4 bitfieldExtract(ivec4, int, int);" ~

        "uint bitfieldExtract(uint, int, int);" ~
        "uvec2 bitfieldExtract(uvec2, int, int);" ~
        "uvec3 bitfieldExtract(uvec3, int, int);" ~
        "uvec4 bitfieldExtract(uvec4, int, int);" ~

        "int bitfieldInsert(int base, int, int, int);" ~
        "ivec2 bitfieldInsert(ivec2 base, ivec2, int, int);" ~
        "ivec3 bitfieldInsert(ivec3 base, ivec3, int, int);" ~
        "ivec4 bitfieldInsert(ivec4 base, ivec4, int, int);" ~

        "uint bitfieldInsert(uint base, uint, int, int);" ~
        "uvec2 bitfieldInsert(uvec2 base, uvec2, int, int);" ~
        "uvec3 bitfieldInsert(uvec3 base, uvec3, int, int);" ~
        "uvec4 bitfieldInsert(uvec4 base, uvec4, int, int);" ~

        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 150) {
      commonBuiltins ~= (
        "int findLSB(int);" ~
        "ivec2 findLSB(ivec2);" ~
        "ivec3 findLSB(ivec3);" ~
        "ivec4 findLSB(ivec4);" ~

        "int findLSB(uint);" ~
        "ivec2 findLSB(uvec2);" ~
        "ivec3 findLSB(uvec3);" ~
        "ivec4 findLSB(uvec4);" ~

        "\n"
      );
    } else if (profile == EProfile(ES_PROFILE: 1) && version_ >= 310) {
      commonBuiltins ~= (
        "lowp int findLSB(int);" ~
        "lowp ivec2 findLSB(ivec2);" ~
        "lowp ivec3 findLSB(ivec3);" ~
        "lowp ivec4 findLSB(ivec4);" ~

        "lowp int findLSB(uint);" ~
        "lowp ivec2 findLSB(uvec2);" ~
        "lowp ivec3 findLSB(uvec3);" ~
        "lowp ivec4 findLSB(uvec4);" ~

        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 150) {
      commonBuiltins ~= (
        "int bitCount(int);" ~
        "ivec2 bitCount(ivec2);" ~
        "ivec3 bitCount(ivec3);" ~
        "ivec4 bitCount(ivec4);" ~

        "int bitCount(uint);" ~
        "ivec2 bitCount(uvec2);" ~
        "ivec3 bitCount(uvec3);" ~
        "ivec4 bitCount(uvec4);" ~

        "int findMSB(highp int);" ~
        "ivec2 findMSB(highp ivec2);" ~
        "ivec3 findMSB(highp ivec3);" ~
        "ivec4 findMSB(highp ivec4);" ~

        "int findMSB(highp uint);" ~
        "ivec2 findMSB(highp uvec2);" ~
        "ivec3 findMSB(highp uvec3);" ~
        "ivec4 findMSB(highp uvec4);" ~
        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 150 && version_ < 450) {
      commonBuiltins ~= (
        "int64_t packInt2x32(ivec2);" ~
        "uint64_t packUint2x32(uvec2);" ~
        "ivec2 unpackInt2x32(int64_t);" ~
        "uvec2 unpackUint2x32(uint64_t);" ~

        "uint packFloat2x16(f16vec2);" ~
        "f16vec2 unpackFloat2x16(uint);" ~

        "int64_t doubleBitsToInt64(double);" ~
        "i64vec2 doubleBitsToInt64(dvec2);" ~
        "i64vec3 doubleBitsToInt64(dvec3);" ~
        "i64vec4 doubleBitsToInt64(dvec4);" ~

        "uint64_t doubleBitsToUint64(double);" ~
        "u64vec2 doubleBitsToUint64(dvec2);" ~
        "u64vec3 doubleBitsToUint64(dvec3);" ~
        "u64vec4 doubleBitsToUint64(dvec4);" ~

        "double int64BitsToDouble(int64_t);" ~
        "dvec2 int64BitsToDouble(i64vec2);" ~
        "dvec3 int64BitsToDouble(i64vec3);" ~
        "dvec4 int64BitsToDouble(i64vec4);" ~

        "double uint64BitsToDouble(uint64_t);" ~
        "dvec2 uint64BitsToDouble(u64vec2);" ~
        "dvec3 uint64BitsToDouble(u64vec3);" ~
        "dvec4 uint64BitsToDouble(u64vec4);" ~

        "bvec2 lessThan(i64vec2, i64vec2);" ~
        "bvec3 lessThan(i64vec3, i64vec3);" ~
        "bvec4 lessThan(i64vec4, i64vec4);" ~
        "bvec2 lessThan(u64vec2, u64vec2);" ~
        "bvec3 lessThan(u64vec3, u64vec3);" ~
        "bvec4 lessThan(u64vec4, u64vec4);" ~

        "bvec2 lessThanEqual(i64vec2, i64vec2);" ~
        "bvec3 lessThanEqual(i64vec3, i64vec3);" ~
        "bvec4 lessThanEqual(i64vec4, i64vec4);" ~
        "bvec2 lessThanEqual(u64vec2, u64vec2);" ~
        "bvec3 lessThanEqual(u64vec3, u64vec3);" ~
        "bvec4 lessThanEqual(u64vec4, u64vec4);" ~

        "bvec2 greaterThan(i64vec2, i64vec2);" ~
        "bvec3 greaterThan(i64vec3, i64vec3);" ~
        "bvec4 greaterThan(i64vec4, i64vec4);" ~
        "bvec2 greaterThan(u64vec2, u64vec2);" ~
        "bvec3 greaterThan(u64vec3, u64vec3);" ~
        "bvec4 greaterThan(u64vec4, u64vec4);" ~

        "bvec2 greaterThanEqual(i64vec2, i64vec2);" ~
        "bvec3 greaterThanEqual(i64vec3, i64vec3);" ~
        "bvec4 greaterThanEqual(i64vec4, i64vec4);" ~
        "bvec2 greaterThanEqual(u64vec2, u64vec2);" ~
        "bvec3 greaterThanEqual(u64vec3, u64vec3);" ~
        "bvec4 greaterThanEqual(u64vec4, u64vec4);" ~

        "bvec2 equal(i64vec2, i64vec2);" ~
        "bvec3 equal(i64vec3, i64vec3);" ~
        "bvec4 equal(i64vec4, i64vec4);" ~
        "bvec2 equal(u64vec2, u64vec2);" ~
        "bvec3 equal(u64vec3, u64vec3);" ~
        "bvec4 equal(u64vec4, u64vec4);" ~

        "bvec2 notEqual(i64vec2, i64vec2);" ~
        "bvec3 notEqual(i64vec3, i64vec3);" ~
        "bvec4 notEqual(i64vec4, i64vec4);" ~
        "bvec2 notEqual(u64vec2, u64vec2);" ~
        "bvec3 notEqual(u64vec3, u64vec3);" ~
        "bvec4 notEqual(u64vec4, u64vec4);" ~

        "bvec2 lessThan(f16vec2, f16vec2);" ~
        "bvec3 lessThan(f16vec3, f16vec3);" ~
        "bvec4 lessThan(f16vec4, f16vec4);" ~

        "bvec2 lessThanEqual(f16vec2, f16vec2);" ~
        "bvec3 lessThanEqual(f16vec3, f16vec3);" ~
        "bvec4 lessThanEqual(f16vec4, f16vec4);" ~

        "bvec2 greaterThan(f16vec2, f16vec2);" ~
        "bvec3 greaterThan(f16vec3, f16vec3);" ~
        "bvec4 greaterThan(f16vec4, f16vec4);" ~

        "bvec2 greaterThanEqual(f16vec2, f16vec2);" ~
        "bvec3 greaterThanEqual(f16vec3, f16vec3);" ~
        "bvec4 greaterThanEqual(f16vec4, f16vec4);" ~

        "bvec2 equal(f16vec2, f16vec2);" ~
        "bvec3 equal(f16vec3, f16vec3);" ~
        "bvec4 equal(f16vec4, f16vec4);" ~

        "bvec2 notEqual(f16vec2, f16vec2);" ~
        "bvec3 notEqual(f16vec3, f16vec3);" ~
        "bvec4 notEqual(f16vec4, f16vec4);" ~

        "bvec2 lessThan(dvec2, dvec2);" ~
        "bvec3 lessThan(dvec3, dvec3);" ~
        "bvec4 lessThan(dvec4, dvec4);" ~

        "bvec2 lessThanEqual(dvec2, dvec2);" ~
        "bvec3 lessThanEqual(dvec3, dvec3);" ~
        "bvec4 lessThanEqual(dvec4, dvec4);" ~

        "bvec2 greaterThan(dvec2, dvec2);" ~
        "bvec3 greaterThan(dvec3, dvec3);" ~
        "bvec4 greaterThan(dvec4, dvec4);" ~

        "bvec2 greaterThanEqual(dvec2, dvec2);" ~
        "bvec3 greaterThanEqual(dvec3, dvec3);" ~
        "bvec4 greaterThanEqual(dvec4, dvec4);" ~

        "bvec2 equal(dvec2, dvec2);" ~
        "bvec3 equal(dvec3, dvec3);" ~
        "bvec4 equal(dvec4, dvec4);" ~

        "bvec2 notEqual(dvec2, dvec2);" ~
        "bvec3 notEqual(dvec3, dvec3);" ~
        "bvec4 notEqual(dvec4, dvec4);" ~

        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 150) {
      commonBuiltins ~= (
        "bool anyThreadNV(bool);" ~
        "bool allThreadsNV(bool);" ~
        "bool allThreadsEqualNV(bool);" ~

        "\n"
      );
    }

    if ((profile == EProfile(ES_PROFILE: 1) && version_ >= 310) ||
      (profile != EProfile(ES_PROFILE: 1) && version_ >= 150)) {
      commonBuiltins ~= (
        "uint uaddCarry(highp uint, highp uint, out lowp uint carry);" ~
        "uvec2 uaddCarry(highp uvec2, highp uvec2, out lowp uvec2 carry);" ~
        "uvec3 uaddCarry(highp uvec3, highp uvec3, out lowp uvec3 carry);" ~
        "uvec4 uaddCarry(highp uvec4, highp uvec4, out lowp uvec4 carry);" ~

        "uint usubBorrow(highp uint, highp uint, out lowp uint borrow);" ~
        "uvec2 usubBorrow(highp uvec2, highp uvec2, out lowp uvec2 borrow);" ~
        "uvec3 usubBorrow(highp uvec3, highp uvec3, out lowp uvec3 borrow);" ~
        "uvec4 usubBorrow(highp uvec4, highp uvec4, out lowp uvec4 borrow);" ~

        "void umulExtended(highp uint, highp uint, out highp uint, out highp uint lsb);" ~
        "void umulExtended(highp uvec2, highp uvec2, out highp uvec2, out highp uvec2 lsb);" ~
        "void umulExtended(highp uvec3, highp uvec3, out highp uvec3, out highp uvec3 lsb);" ~
        "void umulExtended(highp uvec4, highp uvec4, out highp uvec4, out highp uvec4 lsb);" ~

        "void imulExtended(highp int, highp int, out highp int, out highp int lsb);" ~
        "void imulExtended(highp ivec2, highp ivec2, out highp ivec2, out highp ivec2 lsb);" ~
        "void imulExtended(highp ivec3, highp ivec3, out highp ivec3, out highp ivec3 lsb);" ~
        "void imulExtended(highp ivec4, highp ivec4, out highp ivec4, out highp ivec4 lsb);" ~

        "int bitfieldReverse(highp int);" ~
        "ivec2 bitfieldReverse(highp ivec2);" ~
        "ivec3 bitfieldReverse(highp ivec3);" ~
        "ivec4 bitfieldReverse(highp ivec4);" ~

        "uint bitfieldReverse(highp uint);" ~
        "uvec2 bitfieldReverse(highp uvec2);" ~
        "uvec3 bitfieldReverse(highp uvec3);" ~
        "uvec4 bitfieldReverse(highp uvec4);" ~

        "\n"
      );
    }

    if (profile == EProfile(ES_PROFILE: 1) && version_ >= 310) {
      commonBuiltins ~= (
        "lowp int bitCount(int);" ~
        "lowp ivec2 bitCount(ivec2);" ~
        "lowp ivec3 bitCount(ivec3);" ~
        "lowp ivec4 bitCount(ivec4);" ~

        "lowp int bitCount(uint);" ~
        "lowp ivec2 bitCount(uvec2);" ~
        "lowp ivec3 bitCount(uvec3);" ~
        "lowp ivec4 bitCount(uvec4);" ~

        "lowp int findMSB(highp int);" ~
        "lowp ivec2 findMSB(highp ivec2);" ~
        "lowp ivec3 findMSB(highp ivec3);" ~
        "lowp ivec4 findMSB(highp ivec4);" ~

        "lowp int findMSB(highp uint);" ~
        "lowp ivec2 findMSB(highp uvec2);" ~
        "lowp ivec3 findMSB(highp uvec3);" ~
        "lowp ivec4 findMSB(highp uvec4);" ~

        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 450) {
      commonBuiltins ~= (
        "uint64_t ballotARB(bool);" ~

        "float readInvocationARB(float, uint);" ~
        "vec2 readInvocationARB(vec2, uint);" ~
        "vec3 readInvocationARB(vec3, uint);" ~
        "vec4 readInvocationARB(vec4, uint);" ~

        "int readInvocationARB(int, uint);" ~
        "ivec2 readInvocationARB(ivec2, uint);" ~
        "ivec3 readInvocationARB(ivec3, uint);" ~
        "ivec4 readInvocationARB(ivec4, uint);" ~

        "uint readInvocationARB(uint, uint);" ~
        "uvec2 readInvocationARB(uvec2, uint);" ~
        "uvec3 readInvocationARB(uvec3, uint);" ~
        "uvec4 readInvocationARB(uvec4, uint);" ~

        "float readFirstInvocationARB(float);" ~
        "vec2 readFirstInvocationARB(vec2);" ~
        "vec3 readFirstInvocationARB(vec3);" ~
        "vec4 readFirstInvocationARB(vec4);" ~

        "int readFirstInvocationARB(int);" ~
        "ivec2 readFirstInvocationARB(ivec2);" ~
        "ivec3 readFirstInvocationARB(ivec3);" ~
        "ivec4 readFirstInvocationARB(ivec4);" ~

        "uint readFirstInvocationARB(uint);" ~
        "uvec2 readFirstInvocationARB(uvec2);" ~
        "uvec3 readFirstInvocationARB(uvec3);" ~
        "uvec4 readFirstInvocationARB(uvec4);" ~

        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 430) {
      commonBuiltins ~= (
        "bool anyInvocationARB(bool);" ~
        "bool allInvocationsARB(bool);" ~
        "bool allInvocationsEqualARB(bool);" ~

        "\n"
      );
    }

    if ((profile == EProfile(ES_PROFILE: 1) && version_ >= 300) ||
      (profile != EProfile(ES_PROFILE: 1) && version_ >= 450)) {
      commonBuiltins ~= (
        "uint dotEXT(uvec2 a, uvec2 b);" ~
        "int dotEXT(ivec2 a, ivec2 b);" ~
        "int dotEXT(ivec2 a, uvec2 b);" ~
        "int dotEXT(uvec2 a, ivec2 b);" ~

        "uint dotEXT(uvec3 a, uvec3 b);" ~
        "int dotEXT(ivec3 a, ivec3 b);" ~
        "int dotEXT(ivec3 a, uvec3 b);" ~
        "int dotEXT(uvec3 a, ivec3 b);" ~

        "uint dotEXT(uvec4 a, uvec4 b);" ~
        "int dotEXT(ivec4 a, ivec4 b);" ~
        "int dotEXT(ivec4 a, uvec4 b);" ~
        "int dotEXT(uvec4 a, ivec4 b);" ~

        "uint dotPacked4x8EXT(uint a, uint b);" ~
        "int dotPacked4x8EXT(int a, uint b);" ~
        "int dotPacked4x8EXT(uint a, int b);" ~
        "int dotPacked4x8EXT(int a, int b);" ~

        "uint dotEXT(u8vec2 a, u8vec2 b);" ~
        "int dotEXT(i8vec2 a, u8vec2 b);" ~
        "int dotEXT(u8vec2 a, i8vec2 b);" ~
        "int dotEXT(i8vec2 a, i8vec2 b);" ~

        "uint dotEXT(u8vec3 a, u8vec3 b);" ~
        "int dotEXT(i8vec3 a, u8vec3 b);" ~
        "int dotEXT(u8vec3 a, i8vec3 b);" ~
        "int dotEXT(i8vec3 a, i8vec3 b);" ~

        "uint dotEXT(u8vec4 a, u8vec4 b);" ~
        "int dotEXT(i8vec4 a, u8vec4 b);" ~
        "int dotEXT(u8vec4 a, i8vec4 b);" ~
        "int dotEXT(i8vec4 a, i8vec4 b);" ~

        "uint dotEXT(u16vec2 a, u16vec2 b);" ~
        "int dotEXT(i16vec2 a, u16vec2 b);" ~
        "int dotEXT(u16vec2 a, i16vec2 b);" ~
        "int dotEXT(i16vec2 a, i16vec2 b);" ~

        "uint dotEXT(u16vec3 a, u16vec3 b);" ~
        "int dotEXT(i16vec3 a, u16vec3 b);" ~
        "int dotEXT(u16vec3 a, i16vec3 b);" ~
        "int dotEXT(i16vec3 a, i16vec3 b);" ~

        "uint dotEXT(u16vec4 a, u16vec4 b);" ~
        "int dotEXT(i16vec4 a, u16vec4 b);" ~
        "int dotEXT(u16vec4 a, i16vec4 b);" ~
        "int dotEXT(i16vec4 a, i16vec4 b);" ~

        "uint64_t dotEXT(u64vec2 a, u64vec2 b);" ~
        "int64_t dotEXT(i64vec2 a, u64vec2 b);" ~
        "int64_t dotEXT(u64vec2 a, i64vec2 b);" ~
        "int64_t dotEXT(i64vec2 a, i64vec2 b);" ~

        "uint64_t dotEXT(u64vec3 a, u64vec3 b);" ~
        "int64_t dotEXT(i64vec3 a, u64vec3 b);" ~
        "int64_t dotEXT(u64vec3 a, i64vec3 b);" ~
        "int64_t dotEXT(i64vec3 a, i64vec3 b);" ~

        "uint64_t dotEXT(u64vec4 a, u64vec4 b);" ~
        "int64_t dotEXT(i64vec4 a, u64vec4 b);" ~
        "int64_t dotEXT(u64vec4 a, i64vec4 b);" ~
        "int64_t dotEXT(i64vec4 a, i64vec4 b);" ~

        "uint dotAccSatEXT(uvec2 a, uvec2 b, uint c);" ~
        "int dotAccSatEXT(ivec2 a, uvec2 b, int c);" ~
        "int dotAccSatEXT(uvec2 a, ivec2 b, int c);" ~
        "int dotAccSatEXT(ivec2 a, ivec2 b, int c);" ~

        "uint dotAccSatEXT(uvec3 a, uvec3 b, uint c);" ~
        "int dotAccSatEXT(ivec3 a, uvec3 b, int c);" ~
        "int dotAccSatEXT(uvec3 a, ivec3 b, int c);" ~
        "int dotAccSatEXT(ivec3 a, ivec3 b, int c);" ~

        "uint dotAccSatEXT(uvec4 a, uvec4 b, uint c);" ~
        "int dotAccSatEXT(ivec4 a, uvec4 b, int c);" ~
        "int dotAccSatEXT(uvec4 a, ivec4 b, int c);" ~
        "int dotAccSatEXT(ivec4 a, ivec4 b, int c);" ~

        "uint dotPacked4x8AccSatEXT(uint a, uint b, uint c);" ~
        "int dotPacked4x8AccSatEXT(int a, uint b, int c);" ~
        "int dotPacked4x8AccSatEXT(uint a, int b, int c);" ~
        "int dotPacked4x8AccSatEXT(int a, int b, int c);" ~

        "uint dotAccSatEXT(u8vec2 a, u8vec2 b, uint c);" ~
        "int dotAccSatEXT(i8vec2 a, u8vec2 b, int c);" ~
        "int dotAccSatEXT(u8vec2 a, i8vec2 b, int c);" ~
        "int dotAccSatEXT(i8vec2 a, i8vec2 b, int c);" ~

        "uint dotAccSatEXT(u8vec3 a, u8vec3 b, uint c);" ~
        "int dotAccSatEXT(i8vec3 a, u8vec3 b, int c);" ~
        "int dotAccSatEXT(u8vec3 a, i8vec3 b, int c);" ~
        "int dotAccSatEXT(i8vec3 a, i8vec3 b, int c);" ~

        "uint dotAccSatEXT(u8vec4 a, u8vec4 b, uint c);" ~
        "int dotAccSatEXT(i8vec4 a, u8vec4 b, int c);" ~
        "int dotAccSatEXT(u8vec4 a, i8vec4 b, int c);" ~
        "int dotAccSatEXT(i8vec4 a, i8vec4 b, int c);" ~

        "uint dotAccSatEXT(u16vec2 a, u16vec2 b, uint c);" ~
        "int dotAccSatEXT(i16vec2 a, u16vec2 b, int c);" ~
        "int dotAccSatEXT(u16vec2 a, i16vec2 b, int c);" ~
        "int dotAccSatEXT(i16vec2 a, i16vec2 b, int c);" ~

        "uint dotAccSatEXT(u16vec3 a, u16vec3 b, uint c);" ~
        "int dotAccSatEXT(i16vec3 a, u16vec3 b, int c);" ~
        "int dotAccSatEXT(u16vec3 a, i16vec3 b, int c);" ~
        "int dotAccSatEXT(i16vec3 a, i16vec3 b, int c);" ~

        "uint dotAccSatEXT(u16vec4 a, u16vec4 b, uint c);" ~
        "int dotAccSatEXT(i16vec4 a, u16vec4 b, int c);" ~
        "int dotAccSatEXT(u16vec4 a, i16vec4 b, int c);" ~
        "int dotAccSatEXT(i16vec4 a, i16vec4 b, int c);" ~

        "uint64_t dotAccSatEXT(u64vec2 a, u64vec2 b, uint64_t c);" ~
        "int64_t dotAccSatEXT(i64vec2 a, u64vec2 b, int64_t c);" ~
        "int64_t dotAccSatEXT(u64vec2 a, i64vec2 b, int64_t c);" ~
        "int64_t dotAccSatEXT(i64vec2 a, i64vec2 b, int64_t c);" ~

        "uint64_t dotAccSatEXT(u64vec3 a, u64vec3 b, uint64_t c);" ~
        "int64_t dotAccSatEXT(i64vec3 a, u64vec3 b, int64_t c);" ~
        "int64_t dotAccSatEXT(u64vec3 a, i64vec3 b, int64_t c);" ~
        "int64_t dotAccSatEXT(i64vec3 a, i64vec3 b, int64_t c);" ~

        "uint64_t dotAccSatEXT(u64vec4 a, u64vec4 b, uint64_t c);" ~
        "int64_t dotAccSatEXT(i64vec4 a, u64vec4 b, int64_t c);" ~
        "int64_t dotAccSatEXT(u64vec4 a, i64vec4 b, int64_t c);" ~
        "int64_t dotAccSatEXT(i64vec4 a, i64vec4 b, int64_t c);" ~
        "\n"
      );
    }

    if ((profile == EProfile(ES_PROFILE: 1) && version_ >= 310) ||
      (profile != EProfile(ES_PROFILE: 1) && version_ >= 140)) {
      commonBuiltins ~= (
        "void subgroupBarrier();" ~
        "void subgroupMemoryBarrier();" ~
        "void subgroupMemoryBarrierBuffer();" ~
        "void subgroupMemoryBarrierImage();" ~
        "bool subgroupElect();" ~

        "bool subgroupAll(bool);\n" ~
        "bool subgroupAny(bool);\n" ~
        "uvec4 subgroupBallot(bool);\n" ~
        "bool subgroupInverseBallot(uvec4);\n" ~
        "bool subgroupBallotBitExtract(uvec4, uint);\n" ~
        "uint subgroupBallotBitCount(uvec4);\n" ~
        "uint subgroupBallotInclusiveBitCount(uvec4);\n" ~
        "uint subgroupBallotExclusiveBitCount(uvec4);\n" ~
        "uint subgroupBallotFindLSB(uvec4);\n" ~
        "uint subgroupBallotFindMSB(uvec4);\n"
      );

      static foreach (op; subgroupOps) {
        static if (!op.logicalOp) {
          static foreach (floatType; floatTypes)
            commonBuiltins ~= format!op(floatType);
          if (profile != EProfile(ES_PROFILE: 1) && version_ >= 400) {
            static foreach (doubleType; doubleTypes)
              commonBuiltins ~= format!op(doubleType);
          }
        }
        static if (!op.mathOp) {
          static foreach (boolType; boolTypes)
            commonBuiltins ~= format!op(boolType);
        }
        static foreach (intType; intTypes)
          commonBuiltins ~= format!op(intType);
      }

      stageBuiltins.STAGE_COMPUTE ~= (
        "void subgroupMemoryBarrierShared();" ~
        
        "\n"
      );
      stageBuiltins.STAGE_MESH ~= (
        "void subgroupMemoryBarrierShared();" ~
        "\n"
      );
      stageBuiltins.STAGE_TASK ~= (
        "void subgroupMemoryBarrierShared();" ~
        "\n"
      );
    }

    if ((profile == EProfile(ES_PROFILE: 1) && version_ >= 310) ||
      (profile != EProfile(ES_PROFILE: 1) && version_ >= 140)) {
      commonBuiltins ~= (
        "bool subgroupQuadAll(bool);\n" ~
        "bool subgroupQuadAny(bool);\n"
      );   
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 460) {
      commonBuiltins ~= (
        "bool anyInvocation(bool);" ~
        "bool allInvocations(bool);" ~
        "bool allInvocationsEqual(bool);" ~

        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 450) {
      commonBuiltins ~= (
        "float minInvocationsAMD(float);" ~
        "vec2 minInvocationsAMD(vec2);" ~
        "vec3 minInvocationsAMD(vec3);" ~
        "vec4 minInvocationsAMD(vec4);" ~

        "int minInvocationsAMD(int);" ~
        "ivec2 minInvocationsAMD(ivec2);" ~
        "ivec3 minInvocationsAMD(ivec3);" ~
        "ivec4 minInvocationsAMD(ivec4);" ~

        "uint minInvocationsAMD(uint);" ~
        "uvec2 minInvocationsAMD(uvec2);" ~
        "uvec3 minInvocationsAMD(uvec3);" ~
        "uvec4 minInvocationsAMD(uvec4);" ~

        "double minInvocationsAMD(double);" ~
        "dvec2 minInvocationsAMD(dvec2);" ~
        "dvec3 minInvocationsAMD(dvec3);" ~
        "dvec4 minInvocationsAMD(dvec4);" ~

        "int64_t minInvocationsAMD(int64_t);" ~
        "i64vec2 minInvocationsAMD(i64vec2);" ~
        "i64vec3 minInvocationsAMD(i64vec3);" ~
        "i64vec4 minInvocationsAMD(i64vec4);" ~

        "uint64_t minInvocationsAMD(uint64_t);" ~
        "u64vec2 minInvocationsAMD(u64vec2);" ~
        "u64vec3 minInvocationsAMD(u64vec3);" ~
        "u64vec4 minInvocationsAMD(u64vec4);" ~

        "float16_t minInvocationsAMD(float16_t);" ~
        "f16vec2 minInvocationsAMD(f16vec2);" ~
        "f16vec3 minInvocationsAMD(f16vec3);" ~
        "f16vec4 minInvocationsAMD(f16vec4);" ~

        "int16_t minInvocationsAMD(int16_t);" ~
        "i16vec2 minInvocationsAMD(i16vec2);" ~
        "i16vec3 minInvocationsAMD(i16vec3);" ~
        "i16vec4 minInvocationsAMD(i16vec4);" ~

        "uint16_t minInvocationsAMD(uint16_t);" ~
        "u16vec2 minInvocationsAMD(u16vec2);" ~
        "u16vec3 minInvocationsAMD(u16vec3);" ~
        "u16vec4 minInvocationsAMD(u16vec4);" ~

        "float minInvocationsInclusiveScanAMD(float);" ~
        "vec2 minInvocationsInclusiveScanAMD(vec2);" ~
        "vec3 minInvocationsInclusiveScanAMD(vec3);" ~
        "vec4 minInvocationsInclusiveScanAMD(vec4);" ~

        "int minInvocationsInclusiveScanAMD(int);" ~
        "ivec2 minInvocationsInclusiveScanAMD(ivec2);" ~
        "ivec3 minInvocationsInclusiveScanAMD(ivec3);" ~
        "ivec4 minInvocationsInclusiveScanAMD(ivec4);" ~

        "uint minInvocationsInclusiveScanAMD(uint);" ~
        "uvec2 minInvocationsInclusiveScanAMD(uvec2);" ~
        "uvec3 minInvocationsInclusiveScanAMD(uvec3);" ~
        "uvec4 minInvocationsInclusiveScanAMD(uvec4);" ~

        "double minInvocationsInclusiveScanAMD(double);" ~
        "dvec2 minInvocationsInclusiveScanAMD(dvec2);" ~
        "dvec3 minInvocationsInclusiveScanAMD(dvec3);" ~
        "dvec4 minInvocationsInclusiveScanAMD(dvec4);" ~

        "int64_t minInvocationsInclusiveScanAMD(int64_t);" ~
        "i64vec2 minInvocationsInclusiveScanAMD(i64vec2);" ~
        "i64vec3 minInvocationsInclusiveScanAMD(i64vec3);" ~
        "i64vec4 minInvocationsInclusiveScanAMD(i64vec4);" ~

        "uint64_t minInvocationsInclusiveScanAMD(uint64_t);" ~
        "u64vec2 minInvocationsInclusiveScanAMD(u64vec2);" ~
        "u64vec3 minInvocationsInclusiveScanAMD(u64vec3);" ~
        "u64vec4 minInvocationsInclusiveScanAMD(u64vec4);" ~

        "float16_t minInvocationsInclusiveScanAMD(float16_t);" ~
        "f16vec2 minInvocationsInclusiveScanAMD(f16vec2);" ~
        "f16vec3 minInvocationsInclusiveScanAMD(f16vec3);" ~
        "f16vec4 minInvocationsInclusiveScanAMD(f16vec4);" ~

        "int16_t minInvocationsInclusiveScanAMD(int16_t);" ~
        "i16vec2 minInvocationsInclusiveScanAMD(i16vec2);" ~
        "i16vec3 minInvocationsInclusiveScanAMD(i16vec3);" ~
        "i16vec4 minInvocationsInclusiveScanAMD(i16vec4);" ~

        "uint16_t minInvocationsInclusiveScanAMD(uint16_t);" ~
        "u16vec2 minInvocationsInclusiveScanAMD(u16vec2);" ~
        "u16vec3 minInvocationsInclusiveScanAMD(u16vec3);" ~
        "u16vec4 minInvocationsInclusiveScanAMD(u16vec4);" ~

        "float minInvocationsExclusiveScanAMD(float);" ~
        "vec2 minInvocationsExclusiveScanAMD(vec2);" ~
        "vec3 minInvocationsExclusiveScanAMD(vec3);" ~
        "vec4 minInvocationsExclusiveScanAMD(vec4);" ~

        "int minInvocationsExclusiveScanAMD(int);" ~
        "ivec2 minInvocationsExclusiveScanAMD(ivec2);" ~
        "ivec3 minInvocationsExclusiveScanAMD(ivec3);" ~
        "ivec4 minInvocationsExclusiveScanAMD(ivec4);" ~

        "uint minInvocationsExclusiveScanAMD(uint);" ~
        "uvec2 minInvocationsExclusiveScanAMD(uvec2);" ~
        "uvec3 minInvocationsExclusiveScanAMD(uvec3);" ~
        "uvec4 minInvocationsExclusiveScanAMD(uvec4);" ~

        "double minInvocationsExclusiveScanAMD(double);" ~
        "dvec2 minInvocationsExclusiveScanAMD(dvec2);" ~
        "dvec3 minInvocationsExclusiveScanAMD(dvec3);" ~
        "dvec4 minInvocationsExclusiveScanAMD(dvec4);" ~

        "int64_t minInvocationsExclusiveScanAMD(int64_t);" ~
        "i64vec2 minInvocationsExclusiveScanAMD(i64vec2);" ~
        "i64vec3 minInvocationsExclusiveScanAMD(i64vec3);" ~
        "i64vec4 minInvocationsExclusiveScanAMD(i64vec4);" ~

        "uint64_t minInvocationsExclusiveScanAMD(uint64_t);" ~
        "u64vec2 minInvocationsExclusiveScanAMD(u64vec2);" ~
        "u64vec3 minInvocationsExclusiveScanAMD(u64vec3);" ~
        "u64vec4 minInvocationsExclusiveScanAMD(u64vec4);" ~

        "float16_t minInvocationsExclusiveScanAMD(float16_t);" ~
        "f16vec2 minInvocationsExclusiveScanAMD(f16vec2);" ~
        "f16vec3 minInvocationsExclusiveScanAMD(f16vec3);" ~
        "f16vec4 minInvocationsExclusiveScanAMD(f16vec4);" ~

        "int16_t minInvocationsExclusiveScanAMD(int16_t);" ~
        "i16vec2 minInvocationsExclusiveScanAMD(i16vec2);" ~
        "i16vec3 minInvocationsExclusiveScanAMD(i16vec3);" ~
        "i16vec4 minInvocationsExclusiveScanAMD(i16vec4);" ~

        "uint16_t minInvocationsExclusiveScanAMD(uint16_t);" ~
        "u16vec2 minInvocationsExclusiveScanAMD(u16vec2);" ~
        "u16vec3 minInvocationsExclusiveScanAMD(u16vec3);" ~
        "u16vec4 minInvocationsExclusiveScanAMD(u16vec4);" ~

        "float maxInvocationsAMD(float);" ~
        "vec2 maxInvocationsAMD(vec2);" ~
        "vec3 maxInvocationsAMD(vec3);" ~
        "vec4 maxInvocationsAMD(vec4);" ~

        "int maxInvocationsAMD(int);" ~
        "ivec2 maxInvocationsAMD(ivec2);" ~
        "ivec3 maxInvocationsAMD(ivec3);" ~
        "ivec4 maxInvocationsAMD(ivec4);" ~

        "uint maxInvocationsAMD(uint);" ~
        "uvec2 maxInvocationsAMD(uvec2);" ~
        "uvec3 maxInvocationsAMD(uvec3);" ~
        "uvec4 maxInvocationsAMD(uvec4);" ~

        "double maxInvocationsAMD(double);" ~
        "dvec2 maxInvocationsAMD(dvec2);" ~
        "dvec3 maxInvocationsAMD(dvec3);" ~
        "dvec4 maxInvocationsAMD(dvec4);" ~

        "int64_t maxInvocationsAMD(int64_t);" ~
        "i64vec2 maxInvocationsAMD(i64vec2);" ~
        "i64vec3 maxInvocationsAMD(i64vec3);" ~
        "i64vec4 maxInvocationsAMD(i64vec4);" ~

        "uint64_t maxInvocationsAMD(uint64_t);" ~
        "u64vec2 maxInvocationsAMD(u64vec2);" ~
        "u64vec3 maxInvocationsAMD(u64vec3);" ~
        "u64vec4 maxInvocationsAMD(u64vec4);" ~

        "float16_t maxInvocationsAMD(float16_t);" ~
        "f16vec2 maxInvocationsAMD(f16vec2);" ~
        "f16vec3 maxInvocationsAMD(f16vec3);" ~
        "f16vec4 maxInvocationsAMD(f16vec4);" ~

        "int16_t maxInvocationsAMD(int16_t);" ~
        "i16vec2 maxInvocationsAMD(i16vec2);" ~
        "i16vec3 maxInvocationsAMD(i16vec3);" ~
        "i16vec4 maxInvocationsAMD(i16vec4);" ~

        "uint16_t maxInvocationsAMD(uint16_t);" ~
        "u16vec2 maxInvocationsAMD(u16vec2);" ~
        "u16vec3 maxInvocationsAMD(u16vec3);" ~
        "u16vec4 maxInvocationsAMD(u16vec4);" ~

        "float maxInvocationsInclusiveScanAMD(float);" ~
        "vec2 maxInvocationsInclusiveScanAMD(vec2);" ~
        "vec3 maxInvocationsInclusiveScanAMD(vec3);" ~
        "vec4 maxInvocationsInclusiveScanAMD(vec4);" ~

        "int maxInvocationsInclusiveScanAMD(int);" ~
        "ivec2 maxInvocationsInclusiveScanAMD(ivec2);" ~
        "ivec3 maxInvocationsInclusiveScanAMD(ivec3);" ~
        "ivec4 maxInvocationsInclusiveScanAMD(ivec4);" ~

        "uint maxInvocationsInclusiveScanAMD(uint);" ~
        "uvec2 maxInvocationsInclusiveScanAMD(uvec2);" ~
        "uvec3 maxInvocationsInclusiveScanAMD(uvec3);" ~
        "uvec4 maxInvocationsInclusiveScanAMD(uvec4);" ~

        "double maxInvocationsInclusiveScanAMD(double);" ~
        "dvec2 maxInvocationsInclusiveScanAMD(dvec2);" ~
        "dvec3 maxInvocationsInclusiveScanAMD(dvec3);" ~
        "dvec4 maxInvocationsInclusiveScanAMD(dvec4);" ~

        "int64_t maxInvocationsInclusiveScanAMD(int64_t);" ~
        "i64vec2 maxInvocationsInclusiveScanAMD(i64vec2);" ~
        "i64vec3 maxInvocationsInclusiveScanAMD(i64vec3);" ~
        "i64vec4 maxInvocationsInclusiveScanAMD(i64vec4);" ~

        "uint64_t maxInvocationsInclusiveScanAMD(uint64_t);" ~
        "u64vec2 maxInvocationsInclusiveScanAMD(u64vec2);" ~
        "u64vec3 maxInvocationsInclusiveScanAMD(u64vec3);" ~
        "u64vec4 maxInvocationsInclusiveScanAMD(u64vec4);" ~

        "float16_t maxInvocationsInclusiveScanAMD(float16_t);" ~
        "f16vec2 maxInvocationsInclusiveScanAMD(f16vec2);" ~
        "f16vec3 maxInvocationsInclusiveScanAMD(f16vec3);" ~
        "f16vec4 maxInvocationsInclusiveScanAMD(f16vec4);" ~

        "int16_t maxInvocationsInclusiveScanAMD(int16_t);" ~
        "i16vec2 maxInvocationsInclusiveScanAMD(i16vec2);" ~
        "i16vec3 maxInvocationsInclusiveScanAMD(i16vec3);" ~
        "i16vec4 maxInvocationsInclusiveScanAMD(i16vec4);" ~

        "uint16_t maxInvocationsInclusiveScanAMD(uint16_t);" ~
        "u16vec2 maxInvocationsInclusiveScanAMD(u16vec2);" ~
        "u16vec3 maxInvocationsInclusiveScanAMD(u16vec3);" ~
        "u16vec4 maxInvocationsInclusiveScanAMD(u16vec4);" ~

        "float maxInvocationsExclusiveScanAMD(float);" ~
        "vec2 maxInvocationsExclusiveScanAMD(vec2);" ~
        "vec3 maxInvocationsExclusiveScanAMD(vec3);" ~
        "vec4 maxInvocationsExclusiveScanAMD(vec4);" ~

        "int maxInvocationsExclusiveScanAMD(int);" ~
        "ivec2 maxInvocationsExclusiveScanAMD(ivec2);" ~
        "ivec3 maxInvocationsExclusiveScanAMD(ivec3);" ~
        "ivec4 maxInvocationsExclusiveScanAMD(ivec4);" ~

        "uint maxInvocationsExclusiveScanAMD(uint);" ~
        "uvec2 maxInvocationsExclusiveScanAMD(uvec2);" ~
        "uvec3 maxInvocationsExclusiveScanAMD(uvec3);" ~
        "uvec4 maxInvocationsExclusiveScanAMD(uvec4);" ~

        "double maxInvocationsExclusiveScanAMD(double);" ~
        "dvec2 maxInvocationsExclusiveScanAMD(dvec2);" ~
        "dvec3 maxInvocationsExclusiveScanAMD(dvec3);" ~
        "dvec4 maxInvocationsExclusiveScanAMD(dvec4);" ~

        "int64_t maxInvocationsExclusiveScanAMD(int64_t);" ~
        "i64vec2 maxInvocationsExclusiveScanAMD(i64vec2);" ~
        "i64vec3 maxInvocationsExclusiveScanAMD(i64vec3);" ~
        "i64vec4 maxInvocationsExclusiveScanAMD(i64vec4);" ~

        "uint64_t maxInvocationsExclusiveScanAMD(uint64_t);" ~
        "u64vec2 maxInvocationsExclusiveScanAMD(u64vec2);" ~
        "u64vec3 maxInvocationsExclusiveScanAMD(u64vec3);" ~
        "u64vec4 maxInvocationsExclusiveScanAMD(u64vec4);" ~

        "float16_t maxInvocationsExclusiveScanAMD(float16_t);" ~
        "f16vec2 maxInvocationsExclusiveScanAMD(f16vec2);" ~
        "f16vec3 maxInvocationsExclusiveScanAMD(f16vec3);" ~
        "f16vec4 maxInvocationsExclusiveScanAMD(f16vec4);" ~

        "int16_t maxInvocationsExclusiveScanAMD(int16_t);" ~
        "i16vec2 maxInvocationsExclusiveScanAMD(i16vec2);" ~
        "i16vec3 maxInvocationsExclusiveScanAMD(i16vec3);" ~
        "i16vec4 maxInvocationsExclusiveScanAMD(i16vec4);" ~

        "uint16_t maxInvocationsExclusiveScanAMD(uint16_t);" ~
        "u16vec2 maxInvocationsExclusiveScanAMD(u16vec2);" ~
        "u16vec3 maxInvocationsExclusiveScanAMD(u16vec3);" ~
        "u16vec4 maxInvocationsExclusiveScanAMD(u16vec4);" ~

        "float addInvocationsAMD(float);" ~
        "vec2 addInvocationsAMD(vec2);" ~
        "vec3 addInvocationsAMD(vec3);" ~
        "vec4 addInvocationsAMD(vec4);" ~

        "int addInvocationsAMD(int);" ~
        "ivec2 addInvocationsAMD(ivec2);" ~
        "ivec3 addInvocationsAMD(ivec3);" ~
        "ivec4 addInvocationsAMD(ivec4);" ~

        "uint addInvocationsAMD(uint);" ~
        "uvec2 addInvocationsAMD(uvec2);" ~
        "uvec3 addInvocationsAMD(uvec3);" ~
        "uvec4 addInvocationsAMD(uvec4);" ~

        "double addInvocationsAMD(double);" ~
        "dvec2 addInvocationsAMD(dvec2);" ~
        "dvec3 addInvocationsAMD(dvec3);" ~
        "dvec4 addInvocationsAMD(dvec4);" ~

        "int64_t addInvocationsAMD(int64_t);" ~
        "i64vec2 addInvocationsAMD(i64vec2);" ~
        "i64vec3 addInvocationsAMD(i64vec3);" ~
        "i64vec4 addInvocationsAMD(i64vec4);" ~

        "uint64_t addInvocationsAMD(uint64_t);" ~
        "u64vec2 addInvocationsAMD(u64vec2);" ~
        "u64vec3 addInvocationsAMD(u64vec3);" ~
        "u64vec4 addInvocationsAMD(u64vec4);" ~

        "float16_t addInvocationsAMD(float16_t);" ~
        "f16vec2 addInvocationsAMD(f16vec2);" ~
        "f16vec3 addInvocationsAMD(f16vec3);" ~
        "f16vec4 addInvocationsAMD(f16vec4);" ~

        "int16_t addInvocationsAMD(int16_t);" ~
        "i16vec2 addInvocationsAMD(i16vec2);" ~
        "i16vec3 addInvocationsAMD(i16vec3);" ~
        "i16vec4 addInvocationsAMD(i16vec4);" ~

        "uint16_t addInvocationsAMD(uint16_t);" ~
        "u16vec2 addInvocationsAMD(u16vec2);" ~
        "u16vec3 addInvocationsAMD(u16vec3);" ~
        "u16vec4 addInvocationsAMD(u16vec4);" ~

        "float addInvocationsInclusiveScanAMD(float);" ~
        "vec2 addInvocationsInclusiveScanAMD(vec2);" ~
        "vec3 addInvocationsInclusiveScanAMD(vec3);" ~
        "vec4 addInvocationsInclusiveScanAMD(vec4);" ~

        "int addInvocationsInclusiveScanAMD(int);" ~
        "ivec2 addInvocationsInclusiveScanAMD(ivec2);" ~
        "ivec3 addInvocationsInclusiveScanAMD(ivec3);" ~
        "ivec4 addInvocationsInclusiveScanAMD(ivec4);" ~

        "uint addInvocationsInclusiveScanAMD(uint);" ~
        "uvec2 addInvocationsInclusiveScanAMD(uvec2);" ~
        "uvec3 addInvocationsInclusiveScanAMD(uvec3);" ~
        "uvec4 addInvocationsInclusiveScanAMD(uvec4);" ~

        "double addInvocationsInclusiveScanAMD(double);" ~
        "dvec2 addInvocationsInclusiveScanAMD(dvec2);" ~
        "dvec3 addInvocationsInclusiveScanAMD(dvec3);" ~
        "dvec4 addInvocationsInclusiveScanAMD(dvec4);" ~

        "int64_t addInvocationsInclusiveScanAMD(int64_t);" ~
        "i64vec2 addInvocationsInclusiveScanAMD(i64vec2);" ~
        "i64vec3 addInvocationsInclusiveScanAMD(i64vec3);" ~
        "i64vec4 addInvocationsInclusiveScanAMD(i64vec4);" ~

        "uint64_t addInvocationsInclusiveScanAMD(uint64_t);" ~
        "u64vec2 addInvocationsInclusiveScanAMD(u64vec2);" ~
        "u64vec3 addInvocationsInclusiveScanAMD(u64vec3);" ~
        "u64vec4 addInvocationsInclusiveScanAMD(u64vec4);" ~

        "float16_t addInvocationsInclusiveScanAMD(float16_t);" ~
        "f16vec2 addInvocationsInclusiveScanAMD(f16vec2);" ~
        "f16vec3 addInvocationsInclusiveScanAMD(f16vec3);" ~
        "f16vec4 addInvocationsInclusiveScanAMD(f16vec4);" ~

        "int16_t addInvocationsInclusiveScanAMD(int16_t);" ~
        "i16vec2 addInvocationsInclusiveScanAMD(i16vec2);" ~
        "i16vec3 addInvocationsInclusiveScanAMD(i16vec3);" ~
        "i16vec4 addInvocationsInclusiveScanAMD(i16vec4);" ~

        "uint16_t addInvocationsInclusiveScanAMD(uint16_t);" ~
        "u16vec2 addInvocationsInclusiveScanAMD(u16vec2);" ~
        "u16vec3 addInvocationsInclusiveScanAMD(u16vec3);" ~
        "u16vec4 addInvocationsInclusiveScanAMD(u16vec4);" ~

        "float addInvocationsExclusiveScanAMD(float);" ~
        "vec2 addInvocationsExclusiveScanAMD(vec2);" ~
        "vec3 addInvocationsExclusiveScanAMD(vec3);" ~
        "vec4 addInvocationsExclusiveScanAMD(vec4);" ~

        "int addInvocationsExclusiveScanAMD(int);" ~
        "ivec2 addInvocationsExclusiveScanAMD(ivec2);" ~
        "ivec3 addInvocationsExclusiveScanAMD(ivec3);" ~
        "ivec4 addInvocationsExclusiveScanAMD(ivec4);" ~

        "uint addInvocationsExclusiveScanAMD(uint);" ~
        "uvec2 addInvocationsExclusiveScanAMD(uvec2);" ~
        "uvec3 addInvocationsExclusiveScanAMD(uvec3);" ~
        "uvec4 addInvocationsExclusiveScanAMD(uvec4);" ~

        "double addInvocationsExclusiveScanAMD(double);" ~
        "dvec2 addInvocationsExclusiveScanAMD(dvec2);" ~
        "dvec3 addInvocationsExclusiveScanAMD(dvec3);" ~
        "dvec4 addInvocationsExclusiveScanAMD(dvec4);" ~

        "int64_t addInvocationsExclusiveScanAMD(int64_t);" ~
        "i64vec2 addInvocationsExclusiveScanAMD(i64vec2);" ~
        "i64vec3 addInvocationsExclusiveScanAMD(i64vec3);" ~
        "i64vec4 addInvocationsExclusiveScanAMD(i64vec4);" ~

        "uint64_t addInvocationsExclusiveScanAMD(uint64_t);" ~
        "u64vec2 addInvocationsExclusiveScanAMD(u64vec2);" ~
        "u64vec3 addInvocationsExclusiveScanAMD(u64vec3);" ~
        "u64vec4 addInvocationsExclusiveScanAMD(u64vec4);" ~

        "float16_t addInvocationsExclusiveScanAMD(float16_t);" ~
        "f16vec2 addInvocationsExclusiveScanAMD(f16vec2);" ~
        "f16vec3 addInvocationsExclusiveScanAMD(f16vec3);" ~
        "f16vec4 addInvocationsExclusiveScanAMD(f16vec4);" ~

        "int16_t addInvocationsExclusiveScanAMD(int16_t);" ~
        "i16vec2 addInvocationsExclusiveScanAMD(i16vec2);" ~
        "i16vec3 addInvocationsExclusiveScanAMD(i16vec3);" ~
        "i16vec4 addInvocationsExclusiveScanAMD(i16vec4);" ~

        "uint16_t addInvocationsExclusiveScanAMD(uint16_t);" ~
        "u16vec2 addInvocationsExclusiveScanAMD(u16vec2);" ~
        "u16vec3 addInvocationsExclusiveScanAMD(u16vec3);" ~
        "u16vec4 addInvocationsExclusiveScanAMD(u16vec4);" ~

        "float minInvocationsNonUniformAMD(float);" ~
        "vec2 minInvocationsNonUniformAMD(vec2);" ~
        "vec3 minInvocationsNonUniformAMD(vec3);" ~
        "vec4 minInvocationsNonUniformAMD(vec4);" ~

        "int minInvocationsNonUniformAMD(int);" ~
        "ivec2 minInvocationsNonUniformAMD(ivec2);" ~
        "ivec3 minInvocationsNonUniformAMD(ivec3);" ~
        "ivec4 minInvocationsNonUniformAMD(ivec4);" ~

        "uint minInvocationsNonUniformAMD(uint);" ~
        "uvec2 minInvocationsNonUniformAMD(uvec2);" ~
        "uvec3 minInvocationsNonUniformAMD(uvec3);" ~
        "uvec4 minInvocationsNonUniformAMD(uvec4);" ~

        "double minInvocationsNonUniformAMD(double);" ~
        "dvec2 minInvocationsNonUniformAMD(dvec2);" ~
        "dvec3 minInvocationsNonUniformAMD(dvec3);" ~
        "dvec4 minInvocationsNonUniformAMD(dvec4);" ~

        "int64_t minInvocationsNonUniformAMD(int64_t);" ~
        "i64vec2 minInvocationsNonUniformAMD(i64vec2);" ~
        "i64vec3 minInvocationsNonUniformAMD(i64vec3);" ~
        "i64vec4 minInvocationsNonUniformAMD(i64vec4);" ~

        "uint64_t minInvocationsNonUniformAMD(uint64_t);" ~
        "u64vec2 minInvocationsNonUniformAMD(u64vec2);" ~
        "u64vec3 minInvocationsNonUniformAMD(u64vec3);" ~
        "u64vec4 minInvocationsNonUniformAMD(u64vec4);" ~

        "float16_t minInvocationsNonUniformAMD(float16_t);" ~
        "f16vec2 minInvocationsNonUniformAMD(f16vec2);" ~
        "f16vec3 minInvocationsNonUniformAMD(f16vec3);" ~
        "f16vec4 minInvocationsNonUniformAMD(f16vec4);" ~

        "int16_t minInvocationsNonUniformAMD(int16_t);" ~
        "i16vec2 minInvocationsNonUniformAMD(i16vec2);" ~
        "i16vec3 minInvocationsNonUniformAMD(i16vec3);" ~
        "i16vec4 minInvocationsNonUniformAMD(i16vec4);" ~

        "uint16_t minInvocationsNonUniformAMD(uint16_t);" ~
        "u16vec2 minInvocationsNonUniformAMD(u16vec2);" ~
        "u16vec3 minInvocationsNonUniformAMD(u16vec3);" ~
        "u16vec4 minInvocationsNonUniformAMD(u16vec4);" ~

        "float minInvocationsInclusiveScanNonUniformAMD(float);" ~
        "vec2 minInvocationsInclusiveScanNonUniformAMD(vec2);" ~
        "vec3 minInvocationsInclusiveScanNonUniformAMD(vec3);" ~
        "vec4 minInvocationsInclusiveScanNonUniformAMD(vec4);" ~

        "int minInvocationsInclusiveScanNonUniformAMD(int);" ~
        "ivec2 minInvocationsInclusiveScanNonUniformAMD(ivec2);" ~
        "ivec3 minInvocationsInclusiveScanNonUniformAMD(ivec3);" ~
        "ivec4 minInvocationsInclusiveScanNonUniformAMD(ivec4);" ~

        "uint minInvocationsInclusiveScanNonUniformAMD(uint);" ~
        "uvec2 minInvocationsInclusiveScanNonUniformAMD(uvec2);" ~
        "uvec3 minInvocationsInclusiveScanNonUniformAMD(uvec3);" ~
        "uvec4 minInvocationsInclusiveScanNonUniformAMD(uvec4);" ~

        "double minInvocationsInclusiveScanNonUniformAMD(double);" ~
        "dvec2 minInvocationsInclusiveScanNonUniformAMD(dvec2);" ~
        "dvec3 minInvocationsInclusiveScanNonUniformAMD(dvec3);" ~
        "dvec4 minInvocationsInclusiveScanNonUniformAMD(dvec4);" ~

        "int64_t minInvocationsInclusiveScanNonUniformAMD(int64_t);" ~
        "i64vec2 minInvocationsInclusiveScanNonUniformAMD(i64vec2);" ~
        "i64vec3 minInvocationsInclusiveScanNonUniformAMD(i64vec3);" ~
        "i64vec4 minInvocationsInclusiveScanNonUniformAMD(i64vec4);" ~

        "uint64_t minInvocationsInclusiveScanNonUniformAMD(uint64_t);" ~
        "u64vec2 minInvocationsInclusiveScanNonUniformAMD(u64vec2);" ~
        "u64vec3 minInvocationsInclusiveScanNonUniformAMD(u64vec3);" ~
        "u64vec4 minInvocationsInclusiveScanNonUniformAMD(u64vec4);" ~

        "float16_t minInvocationsInclusiveScanNonUniformAMD(float16_t);" ~
        "f16vec2 minInvocationsInclusiveScanNonUniformAMD(f16vec2);" ~
        "f16vec3 minInvocationsInclusiveScanNonUniformAMD(f16vec3);" ~
        "f16vec4 minInvocationsInclusiveScanNonUniformAMD(f16vec4);" ~

        "int16_t minInvocationsInclusiveScanNonUniformAMD(int16_t);" ~
        "i16vec2 minInvocationsInclusiveScanNonUniformAMD(i16vec2);" ~
        "i16vec3 minInvocationsInclusiveScanNonUniformAMD(i16vec3);" ~
        "i16vec4 minInvocationsInclusiveScanNonUniformAMD(i16vec4);" ~

        "uint16_t minInvocationsInclusiveScanNonUniformAMD(uint16_t);" ~
        "u16vec2 minInvocationsInclusiveScanNonUniformAMD(u16vec2);" ~
        "u16vec3 minInvocationsInclusiveScanNonUniformAMD(u16vec3);" ~
        "u16vec4 minInvocationsInclusiveScanNonUniformAMD(u16vec4);" ~

        "float minInvocationsExclusiveScanNonUniformAMD(float);" ~
        "vec2 minInvocationsExclusiveScanNonUniformAMD(vec2);" ~
        "vec3 minInvocationsExclusiveScanNonUniformAMD(vec3);" ~
        "vec4 minInvocationsExclusiveScanNonUniformAMD(vec4);" ~

        "int minInvocationsExclusiveScanNonUniformAMD(int);" ~
        "ivec2 minInvocationsExclusiveScanNonUniformAMD(ivec2);" ~
        "ivec3 minInvocationsExclusiveScanNonUniformAMD(ivec3);" ~
        "ivec4 minInvocationsExclusiveScanNonUniformAMD(ivec4);" ~

        "uint minInvocationsExclusiveScanNonUniformAMD(uint);" ~
        "uvec2 minInvocationsExclusiveScanNonUniformAMD(uvec2);" ~
        "uvec3 minInvocationsExclusiveScanNonUniformAMD(uvec3);" ~
        "uvec4 minInvocationsExclusiveScanNonUniformAMD(uvec4);" ~

        "double minInvocationsExclusiveScanNonUniformAMD(double);" ~
        "dvec2 minInvocationsExclusiveScanNonUniformAMD(dvec2);" ~
        "dvec3 minInvocationsExclusiveScanNonUniformAMD(dvec3);" ~
        "dvec4 minInvocationsExclusiveScanNonUniformAMD(dvec4);" ~

        "int64_t minInvocationsExclusiveScanNonUniformAMD(int64_t);" ~
        "i64vec2 minInvocationsExclusiveScanNonUniformAMD(i64vec2);" ~
        "i64vec3 minInvocationsExclusiveScanNonUniformAMD(i64vec3);" ~
        "i64vec4 minInvocationsExclusiveScanNonUniformAMD(i64vec4);" ~

        "uint64_t minInvocationsExclusiveScanNonUniformAMD(uint64_t);" ~
        "u64vec2 minInvocationsExclusiveScanNonUniformAMD(u64vec2);" ~
        "u64vec3 minInvocationsExclusiveScanNonUniformAMD(u64vec3);" ~
        "u64vec4 minInvocationsExclusiveScanNonUniformAMD(u64vec4);" ~

        "float16_t minInvocationsExclusiveScanNonUniformAMD(float16_t);" ~
        "f16vec2 minInvocationsExclusiveScanNonUniformAMD(f16vec2);" ~
        "f16vec3 minInvocationsExclusiveScanNonUniformAMD(f16vec3);" ~
        "f16vec4 minInvocationsExclusiveScanNonUniformAMD(f16vec4);" ~

        "int16_t minInvocationsExclusiveScanNonUniformAMD(int16_t);" ~
        "i16vec2 minInvocationsExclusiveScanNonUniformAMD(i16vec2);" ~
        "i16vec3 minInvocationsExclusiveScanNonUniformAMD(i16vec3);" ~
        "i16vec4 minInvocationsExclusiveScanNonUniformAMD(i16vec4);" ~

        "uint16_t minInvocationsExclusiveScanNonUniformAMD(uint16_t);" ~
        "u16vec2 minInvocationsExclusiveScanNonUniformAMD(u16vec2);" ~
        "u16vec3 minInvocationsExclusiveScanNonUniformAMD(u16vec3);" ~
        "u16vec4 minInvocationsExclusiveScanNonUniformAMD(u16vec4);" ~

        "float maxInvocationsNonUniformAMD(float);" ~
        "vec2 maxInvocationsNonUniformAMD(vec2);" ~
        "vec3 maxInvocationsNonUniformAMD(vec3);" ~
        "vec4 maxInvocationsNonUniformAMD(vec4);" ~

        "int maxInvocationsNonUniformAMD(int);" ~
        "ivec2 maxInvocationsNonUniformAMD(ivec2);" ~
        "ivec3 maxInvocationsNonUniformAMD(ivec3);" ~
        "ivec4 maxInvocationsNonUniformAMD(ivec4);" ~

        "uint maxInvocationsNonUniformAMD(uint);" ~
        "uvec2 maxInvocationsNonUniformAMD(uvec2);" ~
        "uvec3 maxInvocationsNonUniformAMD(uvec3);" ~
        "uvec4 maxInvocationsNonUniformAMD(uvec4);" ~

        "double maxInvocationsNonUniformAMD(double);" ~
        "dvec2 maxInvocationsNonUniformAMD(dvec2);" ~
        "dvec3 maxInvocationsNonUniformAMD(dvec3);" ~
        "dvec4 maxInvocationsNonUniformAMD(dvec4);" ~

        "int64_t maxInvocationsNonUniformAMD(int64_t);" ~
        "i64vec2 maxInvocationsNonUniformAMD(i64vec2);" ~
        "i64vec3 maxInvocationsNonUniformAMD(i64vec3);" ~
        "i64vec4 maxInvocationsNonUniformAMD(i64vec4);" ~

        "uint64_t maxInvocationsNonUniformAMD(uint64_t);" ~
        "u64vec2 maxInvocationsNonUniformAMD(u64vec2);" ~
        "u64vec3 maxInvocationsNonUniformAMD(u64vec3);" ~
        "u64vec4 maxInvocationsNonUniformAMD(u64vec4);" ~

        "float16_t maxInvocationsNonUniformAMD(float16_t);" ~
        "f16vec2 maxInvocationsNonUniformAMD(f16vec2);" ~
        "f16vec3 maxInvocationsNonUniformAMD(f16vec3);" ~
        "f16vec4 maxInvocationsNonUniformAMD(f16vec4);" ~

        "int16_t maxInvocationsNonUniformAMD(int16_t);" ~
        "i16vec2 maxInvocationsNonUniformAMD(i16vec2);" ~
        "i16vec3 maxInvocationsNonUniformAMD(i16vec3);" ~
        "i16vec4 maxInvocationsNonUniformAMD(i16vec4);" ~

        "uint16_t maxInvocationsNonUniformAMD(uint16_t);" ~
        "u16vec2 maxInvocationsNonUniformAMD(u16vec2);" ~
        "u16vec3 maxInvocationsNonUniformAMD(u16vec3);" ~
        "u16vec4 maxInvocationsNonUniformAMD(u16vec4);" ~

        "float maxInvocationsInclusiveScanNonUniformAMD(float);" ~
        "vec2 maxInvocationsInclusiveScanNonUniformAMD(vec2);" ~
        "vec3 maxInvocationsInclusiveScanNonUniformAMD(vec3);" ~
        "vec4 maxInvocationsInclusiveScanNonUniformAMD(vec4);" ~

        "int maxInvocationsInclusiveScanNonUniformAMD(int);" ~
        "ivec2 maxInvocationsInclusiveScanNonUniformAMD(ivec2);" ~
        "ivec3 maxInvocationsInclusiveScanNonUniformAMD(ivec3);" ~
        "ivec4 maxInvocationsInclusiveScanNonUniformAMD(ivec4);" ~

        "uint maxInvocationsInclusiveScanNonUniformAMD(uint);" ~
        "uvec2 maxInvocationsInclusiveScanNonUniformAMD(uvec2);" ~
        "uvec3 maxInvocationsInclusiveScanNonUniformAMD(uvec3);" ~
        "uvec4 maxInvocationsInclusiveScanNonUniformAMD(uvec4);" ~

        "double maxInvocationsInclusiveScanNonUniformAMD(double);" ~
        "dvec2 maxInvocationsInclusiveScanNonUniformAMD(dvec2);" ~
        "dvec3 maxInvocationsInclusiveScanNonUniformAMD(dvec3);" ~
        "dvec4 maxInvocationsInclusiveScanNonUniformAMD(dvec4);" ~

        "int64_t maxInvocationsInclusiveScanNonUniformAMD(int64_t);" ~
        "i64vec2 maxInvocationsInclusiveScanNonUniformAMD(i64vec2);" ~
        "i64vec3 maxInvocationsInclusiveScanNonUniformAMD(i64vec3);" ~
        "i64vec4 maxInvocationsInclusiveScanNonUniformAMD(i64vec4);" ~

        "uint64_t maxInvocationsInclusiveScanNonUniformAMD(uint64_t);" ~
        "u64vec2 maxInvocationsInclusiveScanNonUniformAMD(u64vec2);" ~
        "u64vec3 maxInvocationsInclusiveScanNonUniformAMD(u64vec3);" ~
        "u64vec4 maxInvocationsInclusiveScanNonUniformAMD(u64vec4);" ~

        "float16_t maxInvocationsInclusiveScanNonUniformAMD(float16_t);" ~
        "f16vec2 maxInvocationsInclusiveScanNonUniformAMD(f16vec2);" ~
        "f16vec3 maxInvocationsInclusiveScanNonUniformAMD(f16vec3);" ~
        "f16vec4 maxInvocationsInclusiveScanNonUniformAMD(f16vec4);" ~

        "int16_t maxInvocationsInclusiveScanNonUniformAMD(int16_t);" ~
        "i16vec2 maxInvocationsInclusiveScanNonUniformAMD(i16vec2);" ~
        "i16vec3 maxInvocationsInclusiveScanNonUniformAMD(i16vec3);" ~
        "i16vec4 maxInvocationsInclusiveScanNonUniformAMD(i16vec4);" ~

        "uint16_t maxInvocationsInclusiveScanNonUniformAMD(uint16_t);" ~
        "u16vec2 maxInvocationsInclusiveScanNonUniformAMD(u16vec2);" ~
        "u16vec3 maxInvocationsInclusiveScanNonUniformAMD(u16vec3);" ~
        "u16vec4 maxInvocationsInclusiveScanNonUniformAMD(u16vec4);" ~

        "float maxInvocationsExclusiveScanNonUniformAMD(float);" ~
        "vec2 maxInvocationsExclusiveScanNonUniformAMD(vec2);" ~
        "vec3 maxInvocationsExclusiveScanNonUniformAMD(vec3);" ~
        "vec4 maxInvocationsExclusiveScanNonUniformAMD(vec4);" ~

        "int maxInvocationsExclusiveScanNonUniformAMD(int);" ~
        "ivec2 maxInvocationsExclusiveScanNonUniformAMD(ivec2);" ~
        "ivec3 maxInvocationsExclusiveScanNonUniformAMD(ivec3);" ~
        "ivec4 maxInvocationsExclusiveScanNonUniformAMD(ivec4);" ~

        "uint maxInvocationsExclusiveScanNonUniformAMD(uint);" ~
        "uvec2 maxInvocationsExclusiveScanNonUniformAMD(uvec2);" ~
        "uvec3 maxInvocationsExclusiveScanNonUniformAMD(uvec3);" ~
        "uvec4 maxInvocationsExclusiveScanNonUniformAMD(uvec4);" ~

        "double maxInvocationsExclusiveScanNonUniformAMD(double);" ~
        "dvec2 maxInvocationsExclusiveScanNonUniformAMD(dvec2);" ~
        "dvec3 maxInvocationsExclusiveScanNonUniformAMD(dvec3);" ~
        "dvec4 maxInvocationsExclusiveScanNonUniformAMD(dvec4);" ~

        "int64_t maxInvocationsExclusiveScanNonUniformAMD(int64_t);" ~
        "i64vec2 maxInvocationsExclusiveScanNonUniformAMD(i64vec2);" ~
        "i64vec3 maxInvocationsExclusiveScanNonUniformAMD(i64vec3);" ~
        "i64vec4 maxInvocationsExclusiveScanNonUniformAMD(i64vec4);" ~

        "uint64_t maxInvocationsExclusiveScanNonUniformAMD(uint64_t);" ~
        "u64vec2 maxInvocationsExclusiveScanNonUniformAMD(u64vec2);" ~
        "u64vec3 maxInvocationsExclusiveScanNonUniformAMD(u64vec3);" ~
        "u64vec4 maxInvocationsExclusiveScanNonUniformAMD(u64vec4);" ~

        "float16_t maxInvocationsExclusiveScanNonUniformAMD(float16_t);" ~
        "f16vec2 maxInvocationsExclusiveScanNonUniformAMD(f16vec2);" ~
        "f16vec3 maxInvocationsExclusiveScanNonUniformAMD(f16vec3);" ~
        "f16vec4 maxInvocationsExclusiveScanNonUniformAMD(f16vec4);" ~

        "int16_t maxInvocationsExclusiveScanNonUniformAMD(int16_t);" ~
        "i16vec2 maxInvocationsExclusiveScanNonUniformAMD(i16vec2);" ~
        "i16vec3 maxInvocationsExclusiveScanNonUniformAMD(i16vec3);" ~
        "i16vec4 maxInvocationsExclusiveScanNonUniformAMD(i16vec4);" ~

        "uint16_t maxInvocationsExclusiveScanNonUniformAMD(uint16_t);" ~
        "u16vec2 maxInvocationsExclusiveScanNonUniformAMD(u16vec2);" ~
        "u16vec3 maxInvocationsExclusiveScanNonUniformAMD(u16vec3);" ~
        "u16vec4 maxInvocationsExclusiveScanNonUniformAMD(u16vec4);" ~

        "float addInvocationsNonUniformAMD(float);" ~
        "vec2 addInvocationsNonUniformAMD(vec2);" ~
        "vec3 addInvocationsNonUniformAMD(vec3);" ~
        "vec4 addInvocationsNonUniformAMD(vec4);" ~

        "int addInvocationsNonUniformAMD(int);" ~
        "ivec2 addInvocationsNonUniformAMD(ivec2);" ~
        "ivec3 addInvocationsNonUniformAMD(ivec3);" ~
        "ivec4 addInvocationsNonUniformAMD(ivec4);" ~

        "uint addInvocationsNonUniformAMD(uint);" ~
        "uvec2 addInvocationsNonUniformAMD(uvec2);" ~
        "uvec3 addInvocationsNonUniformAMD(uvec3);" ~
        "uvec4 addInvocationsNonUniformAMD(uvec4);" ~

        "double addInvocationsNonUniformAMD(double);" ~
        "dvec2 addInvocationsNonUniformAMD(dvec2);" ~
        "dvec3 addInvocationsNonUniformAMD(dvec3);" ~
        "dvec4 addInvocationsNonUniformAMD(dvec4);" ~

        "int64_t addInvocationsNonUniformAMD(int64_t);" ~
        "i64vec2 addInvocationsNonUniformAMD(i64vec2);" ~
        "i64vec3 addInvocationsNonUniformAMD(i64vec3);" ~
        "i64vec4 addInvocationsNonUniformAMD(i64vec4);" ~

        "uint64_t addInvocationsNonUniformAMD(uint64_t);" ~
        "u64vec2 addInvocationsNonUniformAMD(u64vec2);" ~
        "u64vec3 addInvocationsNonUniformAMD(u64vec3);" ~
        "u64vec4 addInvocationsNonUniformAMD(u64vec4);" ~

        "float16_t addInvocationsNonUniformAMD(float16_t);" ~
        "f16vec2 addInvocationsNonUniformAMD(f16vec2);" ~
        "f16vec3 addInvocationsNonUniformAMD(f16vec3);" ~
        "f16vec4 addInvocationsNonUniformAMD(f16vec4);" ~

        "int16_t addInvocationsNonUniformAMD(int16_t);" ~
        "i16vec2 addInvocationsNonUniformAMD(i16vec2);" ~
        "i16vec3 addInvocationsNonUniformAMD(i16vec3);" ~
        "i16vec4 addInvocationsNonUniformAMD(i16vec4);" ~

        "uint16_t addInvocationsNonUniformAMD(uint16_t);" ~
        "u16vec2 addInvocationsNonUniformAMD(u16vec2);" ~
        "u16vec3 addInvocationsNonUniformAMD(u16vec3);" ~
        "u16vec4 addInvocationsNonUniformAMD(u16vec4);" ~

        "float addInvocationsInclusiveScanNonUniformAMD(float);" ~
        "vec2 addInvocationsInclusiveScanNonUniformAMD(vec2);" ~
        "vec3 addInvocationsInclusiveScanNonUniformAMD(vec3);" ~
        "vec4 addInvocationsInclusiveScanNonUniformAMD(vec4);" ~

        "int addInvocationsInclusiveScanNonUniformAMD(int);" ~
        "ivec2 addInvocationsInclusiveScanNonUniformAMD(ivec2);" ~
        "ivec3 addInvocationsInclusiveScanNonUniformAMD(ivec3);" ~
        "ivec4 addInvocationsInclusiveScanNonUniformAMD(ivec4);" ~

        "uint addInvocationsInclusiveScanNonUniformAMD(uint);" ~
        "uvec2 addInvocationsInclusiveScanNonUniformAMD(uvec2);" ~
        "uvec3 addInvocationsInclusiveScanNonUniformAMD(uvec3);" ~
        "uvec4 addInvocationsInclusiveScanNonUniformAMD(uvec4);" ~

        "double addInvocationsInclusiveScanNonUniformAMD(double);" ~
        "dvec2 addInvocationsInclusiveScanNonUniformAMD(dvec2);" ~
        "dvec3 addInvocationsInclusiveScanNonUniformAMD(dvec3);" ~
        "dvec4 addInvocationsInclusiveScanNonUniformAMD(dvec4);" ~

        "int64_t addInvocationsInclusiveScanNonUniformAMD(int64_t);" ~
        "i64vec2 addInvocationsInclusiveScanNonUniformAMD(i64vec2);" ~
        "i64vec3 addInvocationsInclusiveScanNonUniformAMD(i64vec3);" ~
        "i64vec4 addInvocationsInclusiveScanNonUniformAMD(i64vec4);" ~

        "uint64_t addInvocationsInclusiveScanNonUniformAMD(uint64_t);" ~
        "u64vec2 addInvocationsInclusiveScanNonUniformAMD(u64vec2);" ~
        "u64vec3 addInvocationsInclusiveScanNonUniformAMD(u64vec3);" ~
        "u64vec4 addInvocationsInclusiveScanNonUniformAMD(u64vec4);" ~

        "float16_t addInvocationsInclusiveScanNonUniformAMD(float16_t);" ~
        "f16vec2 addInvocationsInclusiveScanNonUniformAMD(f16vec2);" ~
        "f16vec3 addInvocationsInclusiveScanNonUniformAMD(f16vec3);" ~
        "f16vec4 addInvocationsInclusiveScanNonUniformAMD(f16vec4);" ~

        "int16_t addInvocationsInclusiveScanNonUniformAMD(int16_t);" ~
        "i16vec2 addInvocationsInclusiveScanNonUniformAMD(i16vec2);" ~
        "i16vec3 addInvocationsInclusiveScanNonUniformAMD(i16vec3);" ~
        "i16vec4 addInvocationsInclusiveScanNonUniformAMD(i16vec4);" ~

        "uint16_t addInvocationsInclusiveScanNonUniformAMD(uint16_t);" ~
        "u16vec2 addInvocationsInclusiveScanNonUniformAMD(u16vec2);" ~
        "u16vec3 addInvocationsInclusiveScanNonUniformAMD(u16vec3);" ~
        "u16vec4 addInvocationsInclusiveScanNonUniformAMD(u16vec4);" ~

        "float addInvocationsExclusiveScanNonUniformAMD(float);" ~
        "vec2 addInvocationsExclusiveScanNonUniformAMD(vec2);" ~
        "vec3 addInvocationsExclusiveScanNonUniformAMD(vec3);" ~
        "vec4 addInvocationsExclusiveScanNonUniformAMD(vec4);" ~

        "int addInvocationsExclusiveScanNonUniformAMD(int);" ~
        "ivec2 addInvocationsExclusiveScanNonUniformAMD(ivec2);" ~
        "ivec3 addInvocationsExclusiveScanNonUniformAMD(ivec3);" ~
        "ivec4 addInvocationsExclusiveScanNonUniformAMD(ivec4);" ~

        "uint addInvocationsExclusiveScanNonUniformAMD(uint);" ~
        "uvec2 addInvocationsExclusiveScanNonUniformAMD(uvec2);" ~
        "uvec3 addInvocationsExclusiveScanNonUniformAMD(uvec3);" ~
        "uvec4 addInvocationsExclusiveScanNonUniformAMD(uvec4);" ~

        "double addInvocationsExclusiveScanNonUniformAMD(double);" ~
        "dvec2 addInvocationsExclusiveScanNonUniformAMD(dvec2);" ~
        "dvec3 addInvocationsExclusiveScanNonUniformAMD(dvec3);" ~
        "dvec4 addInvocationsExclusiveScanNonUniformAMD(dvec4);" ~

        "int64_t addInvocationsExclusiveScanNonUniformAMD(int64_t);" ~
        "i64vec2 addInvocationsExclusiveScanNonUniformAMD(i64vec2);" ~
        "i64vec3 addInvocationsExclusiveScanNonUniformAMD(i64vec3);" ~
        "i64vec4 addInvocationsExclusiveScanNonUniformAMD(i64vec4);" ~

        "uint64_t addInvocationsExclusiveScanNonUniformAMD(uint64_t);" ~
        "u64vec2 addInvocationsExclusiveScanNonUniformAMD(u64vec2);" ~
        "u64vec3 addInvocationsExclusiveScanNonUniformAMD(u64vec3);" ~
        "u64vec4 addInvocationsExclusiveScanNonUniformAMD(u64vec4);" ~

        "float16_t addInvocationsExclusiveScanNonUniformAMD(float16_t);" ~
        "f16vec2 addInvocationsExclusiveScanNonUniformAMD(f16vec2);" ~
        "f16vec3 addInvocationsExclusiveScanNonUniformAMD(f16vec3);" ~
        "f16vec4 addInvocationsExclusiveScanNonUniformAMD(f16vec4);" ~

        "int16_t addInvocationsExclusiveScanNonUniformAMD(int16_t);" ~
        "i16vec2 addInvocationsExclusiveScanNonUniformAMD(i16vec2);" ~
        "i16vec3 addInvocationsExclusiveScanNonUniformAMD(i16vec3);" ~
        "i16vec4 addInvocationsExclusiveScanNonUniformAMD(i16vec4);" ~

        "uint16_t addInvocationsExclusiveScanNonUniformAMD(uint16_t);" ~
        "u16vec2 addInvocationsExclusiveScanNonUniformAMD(u16vec2);" ~
        "u16vec3 addInvocationsExclusiveScanNonUniformAMD(u16vec3);" ~
        "u16vec4 addInvocationsExclusiveScanNonUniformAMD(u16vec4);" ~

        "float swizzleInvocationsAMD(float, uvec4);" ~
        "vec2 swizzleInvocationsAMD(vec2, uvec4);" ~
        "vec3 swizzleInvocationsAMD(vec3, uvec4);" ~
        "vec4 swizzleInvocationsAMD(vec4, uvec4);" ~

        "int swizzleInvocationsAMD(int, uvec4);" ~
        "ivec2 swizzleInvocationsAMD(ivec2, uvec4);" ~
        "ivec3 swizzleInvocationsAMD(ivec3, uvec4);" ~
        "ivec4 swizzleInvocationsAMD(ivec4, uvec4);" ~

        "uint swizzleInvocationsAMD(uint, uvec4);" ~
        "uvec2 swizzleInvocationsAMD(uvec2, uvec4);" ~
        "uvec3 swizzleInvocationsAMD(uvec3, uvec4);" ~
        "uvec4 swizzleInvocationsAMD(uvec4, uvec4);" ~

        "float swizzleInvocationsMaskedAMD(float, uvec3);" ~
        "vec2 swizzleInvocationsMaskedAMD(vec2, uvec3);" ~
        "vec3 swizzleInvocationsMaskedAMD(vec3, uvec3);" ~
        "vec4 swizzleInvocationsMaskedAMD(vec4, uvec3);" ~

        "int swizzleInvocationsMaskedAMD(int, uvec3);" ~
        "ivec2 swizzleInvocationsMaskedAMD(ivec2, uvec3);" ~
        "ivec3 swizzleInvocationsMaskedAMD(ivec3, uvec3);" ~
        "ivec4 swizzleInvocationsMaskedAMD(ivec4, uvec3);" ~

        "uint swizzleInvocationsMaskedAMD(uint, uvec3);" ~
        "uvec2 swizzleInvocationsMaskedAMD(uvec2, uvec3);" ~
        "uvec3 swizzleInvocationsMaskedAMD(uvec3, uvec3);" ~
        "uvec4 swizzleInvocationsMaskedAMD(uvec4, uvec3);" ~

        "float writeInvocationAMD(float, float, uint);" ~
        "vec2 writeInvocationAMD(vec2, vec2, uint);" ~
        "vec3 writeInvocationAMD(vec3, vec3, uint);" ~
        "vec4 writeInvocationAMD(vec4, vec4, uint);" ~

        "int writeInvocationAMD(int, int, uint);" ~
        "ivec2 writeInvocationAMD(ivec2, ivec2, uint);" ~
        "ivec3 writeInvocationAMD(ivec3, ivec3, uint);" ~
        "ivec4 writeInvocationAMD(ivec4, ivec4, uint);" ~

        "uint writeInvocationAMD(uint, uint, uint);" ~
        "uvec2 writeInvocationAMD(uvec2, uvec2, uint);" ~
        "uvec3 writeInvocationAMD(uvec3, uvec3, uint);" ~
        "uvec4 writeInvocationAMD(uvec4, uvec4, uint);" ~

        "uint mbcntAMD(uint64_t);" ~

        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 440) {
      commonBuiltins ~= (
        "float cubeFaceIndexAMD(vec3);" ~
        "vec2 cubeFaceCoordAMD(vec3);" ~
        "uint64_t timeAMD();" ~

        "in int gl_SIMDGroupSizeAMD;" ~
        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 450) {
      commonBuiltins ~= (
        "uint fragmentMaskFetchAMD(sampler2DMS, ivec2);" ~
        "uint fragmentMaskFetchAMD(isampler2DMS, ivec2);" ~
        "uint fragmentMaskFetchAMD(usampler2DMS, ivec2);" ~

        "uint fragmentMaskFetchAMD(sampler2DMSArray, ivec3);" ~
        "uint fragmentMaskFetchAMD(isampler2DMSArray, ivec3);" ~
        "uint fragmentMaskFetchAMD(usampler2DMSArray, ivec3);" ~

        "vec4 fragmentFetchAMD(sampler2DMS, ivec2, uint);" ~
        "ivec4 fragmentFetchAMD(isampler2DMS, ivec2, uint);" ~
        "uvec4 fragmentFetchAMD(usampler2DMS, ivec2, uint);" ~

        "vec4 fragmentFetchAMD(sampler2DMSArray, ivec3, uint);" ~
        "ivec4 fragmentFetchAMD(isampler2DMSArray, ivec3, uint);" ~
        "uvec4 fragmentFetchAMD(usampler2DMSArray, ivec3, uint);" ~

        "\n"
      );
    }

    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 130) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 300)) {
      commonBuiltins ~= (
        "uint countLeadingZeros(uint);" ~
        "uvec2 countLeadingZeros(uvec2);" ~
        "uvec3 countLeadingZeros(uvec3);" ~
        "uvec4 countLeadingZeros(uvec4);" ~

        "uint countTrailingZeros(uint);" ~
        "uvec2 countTrailingZeros(uvec2);" ~
        "uvec3 countTrailingZeros(uvec3);" ~
        "uvec4 countTrailingZeros(uvec4);" ~

        "uint absoluteDifference(int, int);" ~
        "uvec2 absoluteDifference(ivec2, ivec2);" ~
        "uvec3 absoluteDifference(ivec3, ivec3);" ~
        "uvec4 absoluteDifference(ivec4, ivec4);" ~

        "uint16_t absoluteDifference(int16_t, int16_t);" ~
        "u16vec2 absoluteDifference(i16vec2, i16vec2);" ~
        "u16vec3 absoluteDifference(i16vec3, i16vec3);" ~
        "u16vec4 absoluteDifference(i16vec4, i16vec4);" ~

        "uint64_t absoluteDifference(int64_t, int64_t);" ~
        "u64vec2 absoluteDifference(i64vec2, i64vec2);" ~
        "u64vec3 absoluteDifference(i64vec3, i64vec3);" ~
        "u64vec4 absoluteDifference(i64vec4, i64vec4);" ~

        "uint absoluteDifference(uint, uint);" ~
        "uvec2 absoluteDifference(uvec2, uvec2);" ~
        "uvec3 absoluteDifference(uvec3, uvec3);" ~
        "uvec4 absoluteDifference(uvec4, uvec4);" ~

        "uint16_t absoluteDifference(uint16_t, uint16_t);" ~
        "u16vec2 absoluteDifference(u16vec2, u16vec2);" ~
        "u16vec3 absoluteDifference(u16vec3, u16vec3);" ~
        "u16vec4 absoluteDifference(u16vec4, u16vec4);" ~

        "uint64_t absoluteDifference(uint64_t, uint64_t);" ~
        "u64vec2 absoluteDifference(u64vec2, u64vec2);" ~
        "u64vec3 absoluteDifference(u64vec3, u64vec3);" ~
        "u64vec4 absoluteDifference(u64vec4, u64vec4);" ~

        "int addSaturate(int, int);" ~
        "ivec2 addSaturate(ivec2, ivec2);" ~
        "ivec3 addSaturate(ivec3, ivec3);" ~
        "ivec4 addSaturate(ivec4, ivec4);" ~

        "int16_t addSaturate(int16_t, int16_t);" ~
        "i16vec2 addSaturate(i16vec2, i16vec2);" ~
        "i16vec3 addSaturate(i16vec3, i16vec3);" ~
        "i16vec4 addSaturate(i16vec4, i16vec4);" ~

        "int64_t addSaturate(int64_t, int64_t);" ~
        "i64vec2 addSaturate(i64vec2, i64vec2);" ~
        "i64vec3 addSaturate(i64vec3, i64vec3);" ~
        "i64vec4 addSaturate(i64vec4, i64vec4);" ~

        "uint addSaturate(uint, uint);" ~
        "uvec2 addSaturate(uvec2, uvec2);" ~
        "uvec3 addSaturate(uvec3, uvec3);" ~
        "uvec4 addSaturate(uvec4, uvec4);" ~

        "uint16_t addSaturate(uint16_t, uint16_t);" ~
        "u16vec2 addSaturate(u16vec2, u16vec2);" ~
        "u16vec3 addSaturate(u16vec3, u16vec3);" ~
        "u16vec4 addSaturate(u16vec4, u16vec4);" ~

        "uint64_t addSaturate(uint64_t, uint64_t);" ~
        "u64vec2 addSaturate(u64vec2, u64vec2);" ~
        "u64vec3 addSaturate(u64vec3, u64vec3);" ~
        "u64vec4 addSaturate(u64vec4, u64vec4);" ~

        "int subtractSaturate(int, int);" ~
        "ivec2 subtractSaturate(ivec2, ivec2);" ~
        "ivec3 subtractSaturate(ivec3, ivec3);" ~
        "ivec4 subtractSaturate(ivec4, ivec4);" ~

        "int16_t subtractSaturate(int16_t, int16_t);" ~
        "i16vec2 subtractSaturate(i16vec2, i16vec2);" ~
        "i16vec3 subtractSaturate(i16vec3, i16vec3);" ~
        "i16vec4 subtractSaturate(i16vec4, i16vec4);" ~

        "int64_t subtractSaturate(int64_t, int64_t);" ~
        "i64vec2 subtractSaturate(i64vec2, i64vec2);" ~
        "i64vec3 subtractSaturate(i64vec3, i64vec3);" ~
        "i64vec4 subtractSaturate(i64vec4, i64vec4);" ~

        "uint subtractSaturate(uint, uint);" ~
        "uvec2 subtractSaturate(uvec2, uvec2);" ~
        "uvec3 subtractSaturate(uvec3, uvec3);" ~
        "uvec4 subtractSaturate(uvec4, uvec4);" ~

        "uint16_t subtractSaturate(uint16_t, uint16_t);" ~
        "u16vec2 subtractSaturate(u16vec2, u16vec2);" ~
        "u16vec3 subtractSaturate(u16vec3, u16vec3);" ~
        "u16vec4 subtractSaturate(u16vec4, u16vec4);" ~

        "uint64_t subtractSaturate(uint64_t, uint64_t);" ~
        "u64vec2 subtractSaturate(u64vec2, u64vec2);" ~
        "u64vec3 subtractSaturate(u64vec3, u64vec3);" ~
        "u64vec4 subtractSaturate(u64vec4, u64vec4);" ~

        "int average(int, int);" ~
        "ivec2 average(ivec2, ivec2);" ~
        "ivec3 average(ivec3, ivec3);" ~
        "ivec4 average(ivec4, ivec4);" ~

        "int16_t average(int16_t, int16_t);" ~
        "i16vec2 average(i16vec2, i16vec2);" ~
        "i16vec3 average(i16vec3, i16vec3);" ~
        "i16vec4 average(i16vec4, i16vec4);" ~

        "int64_t average(int64_t, int64_t);" ~
        "i64vec2 average(i64vec2, i64vec2);" ~
        "i64vec3 average(i64vec3, i64vec3);" ~
        "i64vec4 average(i64vec4, i64vec4);" ~

        "uint average(uint, uint);" ~
        "uvec2 average(uvec2, uvec2);" ~
        "uvec3 average(uvec3, uvec3);" ~
        "uvec4 average(uvec4, uvec4);" ~

        "uint16_t average(uint16_t, uint16_t);" ~
        "u16vec2 average(u16vec2, u16vec2);" ~
        "u16vec3 average(u16vec3, u16vec3);" ~
        "u16vec4 average(u16vec4, u16vec4);" ~

        "uint64_t average(uint64_t, uint64_t);" ~
        "u64vec2 average(u64vec2, u64vec2);" ~
        "u64vec3 average(u64vec3, u64vec3);" ~
        "u64vec4 average(u64vec4, u64vec4);" ~

        "int averageRounded(int, int);" ~
        "ivec2 averageRounded(ivec2, ivec2);" ~
        "ivec3 averageRounded(ivec3, ivec3);" ~
        "ivec4 averageRounded(ivec4, ivec4);" ~

        "int16_t averageRounded(int16_t, int16_t);" ~
        "i16vec2 averageRounded(i16vec2, i16vec2);" ~
        "i16vec3 averageRounded(i16vec3, i16vec3);" ~
        "i16vec4 averageRounded(i16vec4, i16vec4);" ~

        "int64_t averageRounded(int64_t, int64_t);" ~
        "i64vec2 averageRounded(i64vec2, i64vec2);" ~
        "i64vec3 averageRounded(i64vec3, i64vec3);" ~
        "i64vec4 averageRounded(i64vec4, i64vec4);" ~

        "uint averageRounded(uint, uint);" ~
        "uvec2 averageRounded(uvec2, uvec2);" ~
        "uvec3 averageRounded(uvec3, uvec3);" ~
        "uvec4 averageRounded(uvec4, uvec4);" ~

        "uint16_t averageRounded(uint16_t, uint16_t);" ~
        "u16vec2 averageRounded(u16vec2, u16vec2);" ~
        "u16vec3 averageRounded(u16vec3, u16vec3);" ~
        "u16vec4 averageRounded(u16vec4, u16vec4);" ~

        "uint64_t averageRounded(uint64_t, uint64_t);" ~
        "u64vec2 averageRounded(u64vec2, u64vec2);" ~
        "u64vec3 averageRounded(u64vec3, u64vec3);" ~
        "u64vec4 averageRounded(u64vec4, u64vec4);" ~

        "int multiply32x16(int, int);" ~
        "ivec2 multiply32x16(ivec2, ivec2);" ~
        "ivec3 multiply32x16(ivec3, ivec3);" ~
        "ivec4 multiply32x16(ivec4, ivec4);" ~

        "uint multiply32x16(uint, uint);" ~
        "uvec2 multiply32x16(uvec2, uvec2);" ~
        "uvec3 multiply32x16(uvec3, uvec3);" ~
        "uvec4 multiply32x16(uvec4, uvec4);" ~
        "\n"
      );
    }

    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 450) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 320)) {
      commonBuiltins ~= (
        "struct gl_TextureFootprint2DNV {" ~
          "uvec2 anchor;" ~
          "uvec2 offset;" ~
          "uvec2 mask;" ~
          "uint lod;" ~
          "uint granularity;" ~
        "};" ~

        "struct gl_TextureFootprint3DNV {" ~
          "uvec3 anchor;" ~
          "uvec3 offset;" ~
          "uvec2 mask;" ~
          "uint lod;" ~
          "uint granularity;" ~
        "};" ~

        "bool textureFootprintNV(sampler2D, vec2, int, bool, out gl_TextureFootprint2DNV);" ~
        "bool textureFootprintNV(sampler3D, vec3, int, bool, out gl_TextureFootprint3DNV);" ~
        "bool textureFootprintNV(sampler2D, vec2, int, bool, out gl_TextureFootprint2DNV, float);" ~
        "bool textureFootprintNV(sampler3D, vec3, int, bool, out gl_TextureFootprint3DNV, float);" ~
        "bool textureFootprintClampNV(sampler2D, vec2, float, int, bool, out gl_TextureFootprint2DNV);" ~
        "bool textureFootprintClampNV(sampler3D, vec3, float, int, bool, out gl_TextureFootprint3DNV);" ~
        "bool textureFootprintClampNV(sampler2D, vec2, float, int, bool, out gl_TextureFootprint2DNV, float);" ~
        "bool textureFootprintClampNV(sampler3D, vec3, float, int, bool, out gl_TextureFootprint3DNV, float);" ~
        "bool textureFootprintLodNV(sampler2D, vec2, float, int, bool, out gl_TextureFootprint2DNV);" ~
        "bool textureFootprintLodNV(sampler3D, vec3, float, int, bool, out gl_TextureFootprint3DNV);" ~
        "bool textureFootprintGradNV(sampler2D, vec2, vec2, vec2, int, bool, out gl_TextureFootprint2DNV);" ~
        "bool textureFootprintGradClampNV(sampler2D, vec2, vec2, vec2, float, int, bool, out gl_TextureFootprint2DNV);" ~
        "\n"
      );   
    }

    if ((profile == EProfile(ES_PROFILE: 1) && version_ >= 300 && version_ < 310) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 150 && version_ < 450)) {
      commonBuiltins ~= (
        "int mix(int, int, bool);" ~
        "ivec2 mix(ivec2, ivec2, bvec2);" ~
        "ivec3 mix(ivec3, ivec3, bvec3);" ~
        "ivec4 mix(ivec4, ivec4, bvec4);" ~
        "uint mix(uint, uint, bool);" ~
        "uvec2 mix(uvec2, uvec2, bvec2);" ~
        "uvec3 mix(uvec3, uvec3, bvec3);" ~
        "uvec4 mix(uvec4, uvec4, bvec4);" ~
        "bool mix(bool, bool, bool);" ~
        "bvec2 mix(bvec2, bvec2, bvec2);" ~
        "bvec3 mix(bvec3, bvec3, bvec3);" ~
        "bvec4 mix(bvec4, bvec4, bvec4);" ~

        "\n"
      );
    }

    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 450) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 310)) {
      commonBuiltins ~= (
        "float16_t radians(float16_t);" ~
        "f16vec2 radians(f16vec2);" ~
        "f16vec3 radians(f16vec3);" ~
        "f16vec4 radians(f16vec4);" ~

        "float16_t degrees(float16_t);" ~
        "f16vec2 degrees(f16vec2);" ~
        "f16vec3 degrees(f16vec3);" ~
        "f16vec4 degrees(f16vec4);" ~

        "float16_t sin(float16_t);" ~
        "f16vec2 sin(f16vec2);" ~
        "f16vec3 sin(f16vec3);" ~
        "f16vec4 sin(f16vec4);" ~

        "float16_t cos(float16_t);" ~
        "f16vec2 cos(f16vec2);" ~
        "f16vec3 cos(f16vec3);" ~
        "f16vec4 cos(f16vec4);" ~

        "float16_t tan(float16_t);" ~
        "f16vec2 tan(f16vec2);" ~
        "f16vec3 tan(f16vec3);" ~
        "f16vec4 tan(f16vec4);" ~

        "float16_t asin(float16_t);" ~
        "f16vec2 asin(f16vec2);" ~
        "f16vec3 asin(f16vec3);" ~
        "f16vec4 asin(f16vec4);" ~

        "float16_t acos(float16_t);" ~
        "f16vec2 acos(f16vec2);" ~
        "f16vec3 acos(f16vec3);" ~
        "f16vec4 acos(f16vec4);" ~

        "float16_t atan(float16_t, float16_t);" ~
        "f16vec2 atan(f16vec2, f16vec2);" ~
        "f16vec3 atan(f16vec3, f16vec3);" ~
        "f16vec4 atan(f16vec4, f16vec4);" ~

        "float16_t atan(float16_t);" ~
        "f16vec2 atan(f16vec2);" ~
        "f16vec3 atan(f16vec3);" ~
        "f16vec4 atan(f16vec4);" ~

        "float16_t sinh(float16_t);" ~
        "f16vec2 sinh(f16vec2);" ~
        "f16vec3 sinh(f16vec3);" ~
        "f16vec4 sinh(f16vec4);" ~

        "float16_t cosh(float16_t);" ~
        "f16vec2 cosh(f16vec2);" ~
        "f16vec3 cosh(f16vec3);" ~
        "f16vec4 cosh(f16vec4);" ~

        "float16_t tanh(float16_t);" ~
        "f16vec2 tanh(f16vec2);" ~
        "f16vec3 tanh(f16vec3);" ~
        "f16vec4 tanh(f16vec4);" ~

        "float16_t asinh(float16_t);" ~
        "f16vec2 asinh(f16vec2);" ~
        "f16vec3 asinh(f16vec3);" ~
        "f16vec4 asinh(f16vec4);" ~

        "float16_t acosh(float16_t);" ~
        "f16vec2 acosh(f16vec2);" ~
        "f16vec3 acosh(f16vec3);" ~
        "f16vec4 acosh(f16vec4);" ~

        "float16_t atanh(float16_t);" ~
        "f16vec2 atanh(f16vec2);" ~
        "f16vec3 atanh(f16vec3);" ~
        "f16vec4 atanh(f16vec4);" ~

        "float16_t pow(float16_t, float16_t);" ~
        "f16vec2 pow(f16vec2, f16vec2);" ~
        "f16vec3 pow(f16vec3, f16vec3);" ~
        "f16vec4 pow(f16vec4, f16vec4);" ~

        "float16_t exp(float16_t);" ~
        "f16vec2 exp(f16vec2);" ~
        "f16vec3 exp(f16vec3);" ~
        "f16vec4 exp(f16vec4);" ~

        "float16_t log(float16_t);" ~
        "f16vec2 log(f16vec2);" ~
        "f16vec3 log(f16vec3);" ~
        "f16vec4 log(f16vec4);" ~

        "float16_t exp2(float16_t);" ~
        "f16vec2 exp2(f16vec2);" ~
        "f16vec3 exp2(f16vec3);" ~
        "f16vec4 exp2(f16vec4);" ~

        "float16_t log2(float16_t);" ~
        "f16vec2 log2(f16vec2);" ~
        "f16vec3 log2(f16vec3);" ~
        "f16vec4 log2(f16vec4);" ~

        "float16_t sqrt(float16_t);" ~
        "f16vec2 sqrt(f16vec2);" ~
        "f16vec3 sqrt(f16vec3);" ~
        "f16vec4 sqrt(f16vec4);" ~

        "float16_t inversesqrt(float16_t);" ~
        "f16vec2 inversesqrt(f16vec2);" ~
        "f16vec3 inversesqrt(f16vec3);" ~
        "f16vec4 inversesqrt(f16vec4);" ~

        "float16_t abs(float16_t);" ~
        "f16vec2 abs(f16vec2);" ~
        "f16vec3 abs(f16vec3);" ~
        "f16vec4 abs(f16vec4);" ~

        "float16_t sign(float16_t);" ~
        "f16vec2 sign(f16vec2);" ~
        "f16vec3 sign(f16vec3);" ~
        "f16vec4 sign(f16vec4);" ~

        "float16_t floor(float16_t);" ~
        "f16vec2 floor(f16vec2);" ~
        "f16vec3 floor(f16vec3);" ~
        "f16vec4 floor(f16vec4);" ~

        "float16_t trunc(float16_t);" ~
        "f16vec2 trunc(f16vec2);" ~
        "f16vec3 trunc(f16vec3);" ~
        "f16vec4 trunc(f16vec4);" ~

        "float16_t round(float16_t);" ~
        "f16vec2 round(f16vec2);" ~
        "f16vec3 round(f16vec3);" ~
        "f16vec4 round(f16vec4);" ~

        "float16_t roundEven(float16_t);" ~
        "f16vec2 roundEven(f16vec2);" ~
        "f16vec3 roundEven(f16vec3);" ~
        "f16vec4 roundEven(f16vec4);" ~

        "float16_t ceil(float16_t);" ~
        "f16vec2 ceil(f16vec2);" ~
        "f16vec3 ceil(f16vec3);" ~
        "f16vec4 ceil(f16vec4);" ~

        "float16_t fract(float16_t);" ~
        "f16vec2 fract(f16vec2);" ~
        "f16vec3 fract(f16vec3);" ~
        "f16vec4 fract(f16vec4);" ~

        "float16_t mod(float16_t, float16_t);" ~
        "f16vec2 mod(f16vec2, float16_t);" ~
        "f16vec3 mod(f16vec3, float16_t);" ~
        "f16vec4 mod(f16vec4, float16_t);" ~
        "f16vec2 mod(f16vec2, f16vec2);" ~
        "f16vec3 mod(f16vec3, f16vec3);" ~
        "f16vec4 mod(f16vec4, f16vec4);" ~

        "float16_t modf(float16_t, out float16_t);" ~
        "f16vec2 modf(f16vec2, out f16vec2);" ~
        "f16vec3 modf(f16vec3, out f16vec3);" ~
        "f16vec4 modf(f16vec4, out f16vec4);" ~

        "float16_t min(float16_t, float16_t);" ~
        "f16vec2 min(f16vec2, float16_t);" ~
        "f16vec3 min(f16vec3, float16_t);" ~
        "f16vec4 min(f16vec4, float16_t);" ~
        "f16vec2 min(f16vec2, f16vec2);" ~
        "f16vec3 min(f16vec3, f16vec3);" ~
        "f16vec4 min(f16vec4, f16vec4);" ~

        "float16_t max(float16_t, float16_t);" ~
        "f16vec2 max(f16vec2, float16_t);" ~
        "f16vec3 max(f16vec3, float16_t);" ~
        "f16vec4 max(f16vec4, float16_t);" ~
        "f16vec2 max(f16vec2, f16vec2);" ~
        "f16vec3 max(f16vec3, f16vec3);" ~
        "f16vec4 max(f16vec4, f16vec4);" ~

        "float16_t clamp(float16_t, float16_t, float16_t);" ~
        "f16vec2 clamp(f16vec2, float16_t, float16_t);" ~
        "f16vec3 clamp(f16vec3, float16_t, float16_t);" ~
        "f16vec4 clamp(f16vec4, float16_t, float16_t);" ~
        "f16vec2 clamp(f16vec2, f16vec2, f16vec2);" ~
        "f16vec3 clamp(f16vec3, f16vec3, f16vec3);" ~
        "f16vec4 clamp(f16vec4, f16vec4, f16vec4);" ~

        "float16_t mix(float16_t, float16_t, float16_t);" ~
        "f16vec2 mix(f16vec2, f16vec2, float16_t);" ~
        "f16vec3 mix(f16vec3, f16vec3, float16_t);" ~
        "f16vec4 mix(f16vec4, f16vec4, float16_t);" ~
        "f16vec2 mix(f16vec2, f16vec2, f16vec2);" ~
        "f16vec3 mix(f16vec3, f16vec3, f16vec3);" ~
        "f16vec4 mix(f16vec4, f16vec4, f16vec4);" ~
        "float16_t mix(float16_t, float16_t, bool);" ~
        "f16vec2 mix(f16vec2, f16vec2, bvec2);" ~
        "f16vec3 mix(f16vec3, f16vec3, bvec3);" ~
        "f16vec4 mix(f16vec4, f16vec4, bvec4);" ~

        "float16_t step(float16_t, float16_t);" ~
        "f16vec2 step(f16vec2, f16vec2);" ~
        "f16vec3 step(f16vec3, f16vec3);" ~
        "f16vec4 step(f16vec4, f16vec4);" ~
        "f16vec2 step(float16_t, f16vec2);" ~
        "f16vec3 step(float16_t, f16vec3);" ~
        "f16vec4 step(float16_t, f16vec4);" ~

        "float16_t smoothstep(float16_t, float16_t, float16_t);" ~
        "f16vec2 smoothstep(f16vec2, f16vec2, f16vec2);" ~
        "f16vec3 smoothstep(f16vec3, f16vec3, f16vec3);" ~
        "f16vec4 smoothstep(f16vec4, f16vec4, f16vec4);" ~
        "f16vec2 smoothstep(float16_t, float16_t, f16vec2);" ~
        "f16vec3 smoothstep(float16_t, float16_t, f16vec3);" ~
        "f16vec4 smoothstep(float16_t, float16_t, f16vec4);" ~

        "bool isnan(float16_t);" ~
        "bvec2 isnan(f16vec2);" ~
        "bvec3 isnan(f16vec3);" ~
        "bvec4 isnan(f16vec4);" ~

        "bool isinf(float16_t);" ~
        "bvec2 isinf(f16vec2);" ~
        "bvec3 isinf(f16vec3);" ~
        "bvec4 isinf(f16vec4);" ~

        "float16_t fma(float16_t, float16_t, float16_t);" ~
        "f16vec2 fma(f16vec2, f16vec2, f16vec2);" ~
        "f16vec3 fma(f16vec3, f16vec3, f16vec3);" ~
        "f16vec4 fma(f16vec4, f16vec4, f16vec4);" ~

        "float16_t frexp(float16_t, out int);" ~
        "f16vec2 frexp(f16vec2, out ivec2);" ~
        "f16vec3 frexp(f16vec3, out ivec3);" ~
        "f16vec4 frexp(f16vec4, out ivec4);" ~

        "float16_t ldexp(float16_t, in int);" ~
        "f16vec2 ldexp(f16vec2, in ivec2);" ~
        "f16vec3 ldexp(f16vec3, in ivec3);" ~
        "f16vec4 ldexp(f16vec4, in ivec4);" ~

        "uint packFloat2x16(f16vec2);" ~
        "f16vec2 unpackFloat2x16(uint);" ~

        "float16_t length(float16_t);" ~
        "float16_t length(f16vec2);" ~
        "float16_t length(f16vec3);" ~
        "float16_t length(f16vec4);" ~

        "float16_t distance(float16_t, float16_t);" ~
        "float16_t distance(f16vec2, f16vec2);" ~
        "float16_t distance(f16vec3, f16vec3);" ~
        "float16_t distance(f16vec4, f16vec4);" ~

        "float16_t dot(float16_t, float16_t);" ~
        "float16_t dot(f16vec2, f16vec2);" ~
        "float16_t dot(f16vec3, f16vec3);" ~
        "float16_t dot(f16vec4, f16vec4);" ~

        "f16vec3 cross(f16vec3, f16vec3);" ~

        "float16_t normalize(float16_t);" ~
        "f16vec2 normalize(f16vec2);" ~
        "f16vec3 normalize(f16vec3);" ~
        "f16vec4 normalize(f16vec4);" ~

        "float16_t faceforward(float16_t, float16_t, float16_t);" ~
        "f16vec2 faceforward(f16vec2, f16vec2, f16vec2);" ~
        "f16vec3 faceforward(f16vec3, f16vec3, f16vec3);" ~
        "f16vec4 faceforward(f16vec4, f16vec4, f16vec4);" ~

        "float16_t reflect(float16_t, float16_t);" ~
        "f16vec2 reflect(f16vec2, f16vec2);" ~
        "f16vec3 reflect(f16vec3, f16vec3);" ~
        "f16vec4 reflect(f16vec4, f16vec4);" ~

        "float16_t refract(float16_t, float16_t, float16_t);" ~
        "f16vec2 refract(f16vec2, f16vec2, float16_t);" ~
        "f16vec3 refract(f16vec3, f16vec3, float16_t);" ~
        "f16vec4 refract(f16vec4, f16vec4, float16_t);" ~

        "f16mat2 matrixCompMult(f16mat2, f16mat2);" ~
        "f16mat3 matrixCompMult(f16mat3, f16mat3);" ~
        "f16mat4 matrixCompMult(f16mat4, f16mat4);" ~
        "f16mat2x3 matrixCompMult(f16mat2x3, f16mat2x3);" ~
        "f16mat2x4 matrixCompMult(f16mat2x4, f16mat2x4);" ~
        "f16mat3x2 matrixCompMult(f16mat3x2, f16mat3x2);" ~
        "f16mat3x4 matrixCompMult(f16mat3x4, f16mat3x4);" ~
        "f16mat4x2 matrixCompMult(f16mat4x2, f16mat4x2);" ~
        "f16mat4x3 matrixCompMult(f16mat4x3, f16mat4x3);" ~

        "f16mat2 outerProduct(f16vec2, f16vec2);" ~
        "f16mat3 outerProduct(f16vec3, f16vec3);" ~
        "f16mat4 outerProduct(f16vec4, f16vec4);" ~
        "f16mat2x3 outerProduct(f16vec3, f16vec2);" ~
        "f16mat3x2 outerProduct(f16vec2, f16vec3);" ~
        "f16mat2x4 outerProduct(f16vec4, f16vec2);" ~
        "f16mat4x2 outerProduct(f16vec2, f16vec4);" ~
        "f16mat3x4 outerProduct(f16vec4, f16vec3);" ~
        "f16mat4x3 outerProduct(f16vec3, f16vec4);" ~

        "f16mat2 transpose(f16mat2);" ~
        "f16mat3 transpose(f16mat3);" ~
        "f16mat4 transpose(f16mat4);" ~
        "f16mat2x3 transpose(f16mat3x2);" ~
        "f16mat3x2 transpose(f16mat2x3);" ~
        "f16mat2x4 transpose(f16mat4x2);" ~
        "f16mat4x2 transpose(f16mat2x4);" ~
        "f16mat3x4 transpose(f16mat4x3);" ~
        "f16mat4x3 transpose(f16mat3x4);" ~

        "float16_t determinant(f16mat2);" ~
        "float16_t determinant(f16mat3);" ~
        "float16_t determinant(f16mat4);" ~

        "f16mat2 inverse(f16mat2);" ~
        "f16mat3 inverse(f16mat3);" ~
        "f16mat4 inverse(f16mat4);" ~

        "bvec2 lessThan(f16vec2, f16vec2);" ~
        "bvec3 lessThan(f16vec3, f16vec3);" ~
        "bvec4 lessThan(f16vec4, f16vec4);" ~

        "bvec2 lessThanEqual(f16vec2, f16vec2);" ~
        "bvec3 lessThanEqual(f16vec3, f16vec3);" ~
        "bvec4 lessThanEqual(f16vec4, f16vec4);" ~

        "bvec2 greaterThan(f16vec2, f16vec2);" ~
        "bvec3 greaterThan(f16vec3, f16vec3);" ~
        "bvec4 greaterThan(f16vec4, f16vec4);" ~

        "bvec2 greaterThanEqual(f16vec2, f16vec2);" ~
        "bvec3 greaterThanEqual(f16vec3, f16vec3);" ~
        "bvec4 greaterThanEqual(f16vec4, f16vec4);" ~

        "bvec2 equal(f16vec2, f16vec2);" ~
        "bvec3 equal(f16vec3, f16vec3);" ~
        "bvec4 equal(f16vec4, f16vec4);" ~

        "bvec2 notEqual(f16vec2, f16vec2);" ~
        "bvec3 notEqual(f16vec3, f16vec3);" ~
        "bvec4 notEqual(f16vec4, f16vec4);" ~

        "bfloat16_t dot(bfloat16_t, bfloat16_t);" ~
        "bfloat16_t dot(bf16vec2, bf16vec2);" ~
        "bfloat16_t dot(bf16vec3, bf16vec3);" ~
        "bfloat16_t dot(bf16vec4, bf16vec4);" ~

        "int16_t bfloat16BitsToIntEXT(bfloat16_t value);" ~
        "i16vec2 bfloat16BitsToIntEXT(bf16vec2 value);" ~
        "i16vec3 bfloat16BitsToIntEXT(bf16vec3 value);" ~
        "i16vec4 bfloat16BitsToIntEXT(bf16vec4 value);" ~

        "uint16_t bfloat16BitsToUintEXT(bfloat16_t value);" ~
        "u16vec2 bfloat16BitsToUintEXT(bf16vec2 value);" ~
        "u16vec3 bfloat16BitsToUintEXT(bf16vec3 value);" ~
        "u16vec4 bfloat16BitsToUintEXT(bf16vec4 value);" ~

        "bfloat16_t intBitsToBFloat16EXT(int16_t value);" ~
        "bf16vec2 intBitsToBFloat16EXT(i16vec2 value);" ~
        "bf16vec3 intBitsToBFloat16EXT(i16vec3 value);" ~
        "bf16vec4 intBitsToBFloat16EXT(i16vec4 value);" ~

        "bfloat16_t uintBitsToBFloat16EXT(uint16_t value);" ~
        "bf16vec2 uintBitsToBFloat16EXT(u16vec2 value);" ~
        "bf16vec3 uintBitsToBFloat16EXT(u16vec3 value);" ~
        "bf16vec4 uintBitsToBFloat16EXT(u16vec4 value);" ~

        "int8_t floate5m2BitsToIntEXT(floate5m2_t value);" ~
        "i8vec2 floate5m2BitsToIntEXT(fe5m2vec2 value);" ~
        "i8vec3 floate5m2BitsToIntEXT(fe5m2vec3 value);" ~
        "i8vec4 floate5m2BitsToIntEXT(fe5m2vec4 value);" ~

        "uint8_t floate5m2BitsToUintEXT(floate5m2_t value);" ~
        "u8vec2 floate5m2BitsToUintEXT(fe5m2vec2 value);" ~
        "u8vec3 floate5m2BitsToUintEXT(fe5m2vec3 value);" ~
        "u8vec4 floate5m2BitsToUintEXT(fe5m2vec4 value);" ~

        "floate5m2_t intBitsToFloate5m2EXT(int8_t value);" ~
        "fe5m2vec2 intBitsToFloate5m2EXT(i8vec2 value);" ~
        "fe5m2vec3 intBitsToFloate5m2EXT(i8vec3 value);" ~
        "fe5m2vec4 intBitsToFloate5m2EXT(i8vec4 value);" ~

        "floate5m2_t uintBitsToFloate5m2EXT(uint8_t value);" ~
        "fe5m2vec2 uintBitsToFloate5m2EXT(u8vec2 value);" ~
        "fe5m2vec3 uintBitsToFloate5m2EXT(u8vec3 value);" ~
        "fe5m2vec4 uintBitsToFloate5m2EXT(u8vec4 value);" ~

        "int8_t floate4m3BitsToIntEXT(floate4m3_t value);" ~
        "i8vec2 floate4m3BitsToIntEXT(fe4m3vec2 value);" ~
        "i8vec3 floate4m3BitsToIntEXT(fe4m3vec3 value);" ~
        "i8vec4 floate4m3BitsToIntEXT(fe4m3vec4 value);" ~

        "uint8_t floate4m3BitsToUintEXT(floate4m3_t value);" ~
        "u8vec2 floate4m3BitsToUintEXT(fe4m3vec2 value);" ~
        "u8vec3 floate4m3BitsToUintEXT(fe4m3vec3 value);" ~
        "u8vec4 floate4m3BitsToUintEXT(fe4m3vec4 value);" ~

        "floate4m3_t intBitsToFloate4m3EXT(int8_t value);" ~
        "fe4m3vec2 intBitsToFloate4m3EXT(i8vec2 value);" ~
        "fe4m3vec3 intBitsToFloate4m3EXT(i8vec3 value);" ~
        "fe4m3vec4 intBitsToFloate4m3EXT(i8vec4 value);" ~

        "floate4m3_t uintBitsToFloate4m3EXT(uint8_t value);" ~
        "fe4m3vec2 uintBitsToFloate4m3EXT(u8vec2 value);" ~
        "fe4m3vec3 uintBitsToFloate4m3EXT(u8vec3 value);" ~
        "fe4m3vec4 uintBitsToFloate4m3EXT(u8vec4 value);" ~

        "void saturatedConvertEXT();" ~
        "\n"
      );
    }

    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 450) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 310)) {
      commonBuiltins ~= (
        "int8_t abs(int8_t);" ~
        "i8vec2 abs(i8vec2);" ~
        "i8vec3 abs(i8vec3);" ~
        "i8vec4 abs(i8vec4);" ~

        "int8_t sign(int8_t);" ~
        "i8vec2 sign(i8vec2);" ~
        "i8vec3 sign(i8vec3);" ~
        "i8vec4 sign(i8vec4);" ~

        "int8_t min(int8_t x, int8_t y);" ~
        "i8vec2 min(i8vec2 x, int8_t y);" ~
        "i8vec3 min(i8vec3 x, int8_t y);" ~
        "i8vec4 min(i8vec4 x, int8_t y);" ~
        "i8vec2 min(i8vec2 x, i8vec2 y);" ~
        "i8vec3 min(i8vec3 x, i8vec3 y);" ~
        "i8vec4 min(i8vec4 x, i8vec4 y);" ~

        "uint8_t min(uint8_t x, uint8_t y);" ~
        "u8vec2 min(u8vec2 x, uint8_t y);" ~
        "u8vec3 min(u8vec3 x, uint8_t y);" ~
        "u8vec4 min(u8vec4 x, uint8_t y);" ~
        "u8vec2 min(u8vec2 x, u8vec2 y);" ~
        "u8vec3 min(u8vec3 x, u8vec3 y);" ~
        "u8vec4 min(u8vec4 x, u8vec4 y);" ~

        "int8_t max(int8_t x, int8_t y);" ~
        "i8vec2 max(i8vec2 x, int8_t y);" ~
        "i8vec3 max(i8vec3 x, int8_t y);" ~
        "i8vec4 max(i8vec4 x, int8_t y);" ~
        "i8vec2 max(i8vec2 x, i8vec2 y);" ~
        "i8vec3 max(i8vec3 x, i8vec3 y);" ~
        "i8vec4 max(i8vec4 x, i8vec4 y);" ~

        "uint8_t max(uint8_t x, uint8_t y);" ~
        "u8vec2 max(u8vec2 x, uint8_t y);" ~
        "u8vec3 max(u8vec3 x, uint8_t y);" ~
        "u8vec4 max(u8vec4 x, uint8_t y);" ~
        "u8vec2 max(u8vec2 x, u8vec2 y);" ~
        "u8vec3 max(u8vec3 x, u8vec3 y);" ~
        "u8vec4 max(u8vec4 x, u8vec4 y);" ~

        "int8_t clamp(int8_t x, int8_t minVal, int8_t maxVal);" ~
        "i8vec2 clamp(i8vec2 x, int8_t minVal, int8_t maxVal);" ~
        "i8vec3 clamp(i8vec3 x, int8_t minVal, int8_t maxVal);" ~
        "i8vec4 clamp(i8vec4 x, int8_t minVal, int8_t maxVal);" ~
        "i8vec2 clamp(i8vec2 x, i8vec2 minVal, i8vec2 maxVal);" ~
        "i8vec3 clamp(i8vec3 x, i8vec3 minVal, i8vec3 maxVal);" ~
        "i8vec4 clamp(i8vec4 x, i8vec4 minVal, i8vec4 maxVal);" ~

        "uint8_t clamp(uint8_t x, uint8_t minVal, uint8_t maxVal);" ~
        "u8vec2 clamp(u8vec2 x, uint8_t minVal, uint8_t maxVal);" ~
        "u8vec3 clamp(u8vec3 x, uint8_t minVal, uint8_t maxVal);" ~
        "u8vec4 clamp(u8vec4 x, uint8_t minVal, uint8_t maxVal);" ~
        "u8vec2 clamp(u8vec2 x, u8vec2 minVal, u8vec2 maxVal);" ~
        "u8vec3 clamp(u8vec3 x, u8vec3 minVal, u8vec3 maxVal);" ~
        "u8vec4 clamp(u8vec4 x, u8vec4 minVal, u8vec4 maxVal);" ~

        "int8_t mix(int8_t,int8_t, bool);" ~
        "i8vec2 mix(i8vec2, i8vec2, bvec2);" ~
        "i8vec3 mix(i8vec3, i8vec3, bvec3);" ~
        "i8vec4 mix(i8vec4, i8vec4, bvec4);" ~
        "uint8_t mix(uint8_t, uint8_t, bool);" ~
        "u8vec2 mix(u8vec2, u8vec2, bvec2);" ~
        "u8vec3 mix(u8vec3, u8vec3, bvec3);" ~
        "u8vec4 mix(u8vec4, u8vec4, bvec4);" ~

        "bvec2 lessThan(i8vec2, i8vec2);" ~
        "bvec3 lessThan(i8vec3, i8vec3);" ~
        "bvec4 lessThan(i8vec4, i8vec4);" ~
        "bvec2 lessThan(u8vec2, u8vec2);" ~
        "bvec3 lessThan(u8vec3, u8vec3);" ~
        "bvec4 lessThan(u8vec4, u8vec4);" ~

        "bvec2 lessThanEqual(i8vec2, i8vec2);" ~
        "bvec3 lessThanEqual(i8vec3, i8vec3);" ~
        "bvec4 lessThanEqual(i8vec4, i8vec4);" ~
        "bvec2 lessThanEqual(u8vec2, u8vec2);" ~
        "bvec3 lessThanEqual(u8vec3, u8vec3);" ~
        "bvec4 lessThanEqual(u8vec4, u8vec4);" ~

        "bvec2 greaterThan(i8vec2, i8vec2);" ~
        "bvec3 greaterThan(i8vec3, i8vec3);" ~
        "bvec4 greaterThan(i8vec4, i8vec4);" ~
        "bvec2 greaterThan(u8vec2, u8vec2);" ~
        "bvec3 greaterThan(u8vec3, u8vec3);" ~
        "bvec4 greaterThan(u8vec4, u8vec4);" ~

        "bvec2 greaterThanEqual(i8vec2, i8vec2);" ~
        "bvec3 greaterThanEqual(i8vec3, i8vec3);" ~
        "bvec4 greaterThanEqual(i8vec4, i8vec4);" ~
        "bvec2 greaterThanEqual(u8vec2, u8vec2);" ~
        "bvec3 greaterThanEqual(u8vec3, u8vec3);" ~
        "bvec4 greaterThanEqual(u8vec4, u8vec4);" ~

        "bvec2 equal(i8vec2, i8vec2);" ~
        "bvec3 equal(i8vec3, i8vec3);" ~
        "bvec4 equal(i8vec4, i8vec4);" ~
        "bvec2 equal(u8vec2, u8vec2);" ~
        "bvec3 equal(u8vec3, u8vec3);" ~
        "bvec4 equal(u8vec4, u8vec4);" ~

        "bvec2 notEqual(i8vec2, i8vec2);" ~
        "bvec3 notEqual(i8vec3, i8vec3);" ~
        "bvec4 notEqual(i8vec4, i8vec4);" ~
        "bvec2 notEqual(u8vec2, u8vec2);" ~
        "bvec3 notEqual(u8vec3, u8vec3);" ~
        "bvec4 notEqual(u8vec4, u8vec4);" ~

        "int8_t bitfieldExtract(int8_t, int8_t, int8_t);" ~
        "i8vec2 bitfieldExtract(i8vec2, int8_t, int8_t);" ~
        "i8vec3 bitfieldExtract(i8vec3, int8_t, int8_t);" ~
        "i8vec4 bitfieldExtract(i8vec4, int8_t, int8_t);" ~

        "uint8_t bitfieldExtract(uint8_t, int8_t, int8_t);" ~
        "u8vec2 bitfieldExtract(u8vec2, int8_t, int8_t);" ~
        "u8vec3 bitfieldExtract(u8vec3, int8_t, int8_t);" ~
        "u8vec4 bitfieldExtract(u8vec4, int8_t, int8_t);" ~

        "int8_t bitfieldInsert(int8_t base, int8_t, int8_t, int8_t);" ~
        "i8vec2 bitfieldInsert(i8vec2 base, i8vec2, int8_t, int8_t);" ~
        "i8vec3 bitfieldInsert(i8vec3 base, i8vec3, int8_t, int8_t);" ~
        "i8vec4 bitfieldInsert(i8vec4 base, i8vec4, int8_t, int8_t);" ~

        "uint8_t bitfieldInsert(uint8_t base, uint8_t, int8_t, int8_t);" ~
        "u8vec2 bitfieldInsert(u8vec2 base, u8vec2, int8_t, int8_t);" ~
        "u8vec3 bitfieldInsert(u8vec3 base, u8vec3, int8_t, int8_t);" ~
        "u8vec4 bitfieldInsert(u8vec4 base, u8vec4, int8_t, int8_t);" ~

        "int8_t bitCount(int8_t);" ~
        "i8vec2 bitCount(i8vec2);" ~
        "i8vec3 bitCount(i8vec3);" ~
        "i8vec4 bitCount(i8vec4);" ~

        "int8_t bitCount(uint8_t);" ~
        "i8vec2 bitCount(u8vec2);" ~
        "i8vec3 bitCount(u8vec3);" ~
        "i8vec4 bitCount(u8vec4);" ~

        "int8_t findLSB(int8_t);" ~
        "i8vec2 findLSB(i8vec2);" ~
        "i8vec3 findLSB(i8vec3);" ~
        "i8vec4 findLSB(i8vec4);" ~

        "int8_t findLSB(uint8_t);" ~
        "i8vec2 findLSB(u8vec2);" ~
        "i8vec3 findLSB(u8vec3);" ~
        "i8vec4 findLSB(u8vec4);" ~

        "int8_t findMSB(int8_t);" ~
        "i8vec2 findMSB(i8vec2);" ~
        "i8vec3 findMSB(i8vec3);" ~
        "i8vec4 findMSB(i8vec4);" ~

        "int8_t findMSB(uint8_t);" ~
        "i8vec2 findMSB(u8vec2);" ~
        "i8vec3 findMSB(u8vec3);" ~
        "i8vec4 findMSB(u8vec4);" ~

        "int16_t abs(int16_t);" ~
        "i16vec2 abs(i16vec2);" ~
        "i16vec3 abs(i16vec3);" ~
        "i16vec4 abs(i16vec4);" ~

        "int16_t sign(int16_t);" ~
        "i16vec2 sign(i16vec2);" ~
        "i16vec3 sign(i16vec3);" ~
        "i16vec4 sign(i16vec4);" ~

        "int16_t min(int16_t x, int16_t y);" ~
        "i16vec2 min(i16vec2 x, int16_t y);" ~
        "i16vec3 min(i16vec3 x, int16_t y);" ~
        "i16vec4 min(i16vec4 x, int16_t y);" ~
        "i16vec2 min(i16vec2 x, i16vec2 y);" ~
        "i16vec3 min(i16vec3 x, i16vec3 y);" ~
        "i16vec4 min(i16vec4 x, i16vec4 y);" ~

        "uint16_t min(uint16_t x, uint16_t y);" ~
        "u16vec2 min(u16vec2 x, uint16_t y);" ~
        "u16vec3 min(u16vec3 x, uint16_t y);" ~
        "u16vec4 min(u16vec4 x, uint16_t y);" ~
        "u16vec2 min(u16vec2 x, u16vec2 y);" ~
        "u16vec3 min(u16vec3 x, u16vec3 y);" ~
        "u16vec4 min(u16vec4 x, u16vec4 y);" ~

        "int16_t max(int16_t x, int16_t y);" ~
        "i16vec2 max(i16vec2 x, int16_t y);" ~
        "i16vec3 max(i16vec3 x, int16_t y);" ~
        "i16vec4 max(i16vec4 x, int16_t y);" ~
        "i16vec2 max(i16vec2 x, i16vec2 y);" ~
        "i16vec3 max(i16vec3 x, i16vec3 y);" ~
        "i16vec4 max(i16vec4 x, i16vec4 y);" ~

        "uint16_t max(uint16_t x, uint16_t y);" ~
        "u16vec2 max(u16vec2 x, uint16_t y);" ~
        "u16vec3 max(u16vec3 x, uint16_t y);" ~
        "u16vec4 max(u16vec4 x, uint16_t y);" ~
        "u16vec2 max(u16vec2 x, u16vec2 y);" ~
        "u16vec3 max(u16vec3 x, u16vec3 y);" ~
        "u16vec4 max(u16vec4 x, u16vec4 y);" ~

        "int16_t clamp(int16_t x, int16_t minVal, int16_t maxVal);" ~
        "i16vec2 clamp(i16vec2 x, int16_t minVal, int16_t maxVal);" ~
        "i16vec3 clamp(i16vec3 x, int16_t minVal, int16_t maxVal);" ~
        "i16vec4 clamp(i16vec4 x, int16_t minVal, int16_t maxVal);" ~
        "i16vec2 clamp(i16vec2 x, i16vec2 minVal, i16vec2 maxVal);" ~
        "i16vec3 clamp(i16vec3 x, i16vec3 minVal, i16vec3 maxVal);" ~
        "i16vec4 clamp(i16vec4 x, i16vec4 minVal, i16vec4 maxVal);" ~

        "uint16_t clamp(uint16_t x, uint16_t minVal, uint16_t maxVal);" ~
        "u16vec2 clamp(u16vec2 x, uint16_t minVal, uint16_t maxVal);" ~
        "u16vec3 clamp(u16vec3 x, uint16_t minVal, uint16_t maxVal);" ~
        "u16vec4 clamp(u16vec4 x, uint16_t minVal, uint16_t maxVal);" ~
        "u16vec2 clamp(u16vec2 x, u16vec2 minVal, u16vec2 maxVal);" ~
        "u16vec3 clamp(u16vec3 x, u16vec3 minVal, u16vec3 maxVal);" ~
        "u16vec4 clamp(u16vec4 x, u16vec4 minVal, u16vec4 maxVal);" ~

        "int16_t mix(int16_t, int16_t, bool);" ~
        "i16vec2 mix(i16vec2, i16vec2, bvec2);" ~
        "i16vec3 mix(i16vec3, i16vec3, bvec3);" ~
        "i16vec4 mix(i16vec4, i16vec4, bvec4);" ~
        "uint16_t mix(uint16_t, uint16_t, bool);" ~
        "u16vec2 mix(u16vec2, u16vec2, bvec2);" ~
        "u16vec3 mix(u16vec3, u16vec3, bvec3);" ~
        "u16vec4 mix(u16vec4, u16vec4, bvec4);" ~

        "float16_t frexp(float16_t, out int16_t);" ~
        "f16vec2 frexp(f16vec2, out i16vec2);" ~
        "f16vec3 frexp(f16vec3, out i16vec3);" ~
        "f16vec4 frexp(f16vec4, out i16vec4);" ~

        "float16_t ldexp(float16_t, int16_t);" ~
        "f16vec2 ldexp(f16vec2, i16vec2);" ~
        "f16vec3 ldexp(f16vec3, i16vec3);" ~
        "f16vec4 ldexp(f16vec4, i16vec4);" ~

        "int16_t halfBitsToInt16(float16_t);" ~
        "i16vec2 halfBitsToInt16(f16vec2);" ~
        "i16vec3 halhBitsToInt16(f16vec3);" ~
        "i16vec4 halfBitsToInt16(f16vec4);" ~

        "uint16_t halfBitsToUint16(float16_t);" ~
        "u16vec2 halfBitsToUint16(f16vec2);" ~
        "u16vec3 halfBitsToUint16(f16vec3);" ~
        "u16vec4 halfBitsToUint16(f16vec4);" ~

        "int16_t float16BitsToInt16(float16_t);" ~
        "i16vec2 float16BitsToInt16(f16vec2);" ~
        "i16vec3 float16BitsToInt16(f16vec3);" ~
        "i16vec4 float16BitsToInt16(f16vec4);" ~

        "uint16_t float16BitsToUint16(float16_t);" ~
        "u16vec2 float16BitsToUint16(f16vec2);" ~
        "u16vec3 float16BitsToUint16(f16vec3);" ~
        "u16vec4 float16BitsToUint16(f16vec4);" ~

        "float16_t int16BitsToFloat16(int16_t);" ~
        "f16vec2 int16BitsToFloat16(i16vec2);" ~
        "f16vec3 int16BitsToFloat16(i16vec3);" ~
        "f16vec4 int16BitsToFloat16(i16vec4);" ~

        "float16_t uint16BitsToFloat16(uint16_t);" ~
        "f16vec2 uint16BitsToFloat16(u16vec2);" ~
        "f16vec3 uint16BitsToFloat16(u16vec3);" ~
        "f16vec4 uint16BitsToFloat16(u16vec4);" ~

        "float16_t int16BitsToHalf(int16_t);" ~
        "f16vec2 int16BitsToHalf(i16vec2);" ~
        "f16vec3 int16BitsToHalf(i16vec3);" ~
        "f16vec4 int16BitsToHalf(i16vec4);" ~

        "float16_t uint16BitsToHalf(uint16_t);" ~
        "f16vec2 uint16BitsToHalf(u16vec2);" ~
        "f16vec3 uint16BitsToHalf(u16vec3);" ~
        "f16vec4 uint16BitsToHalf(u16vec4);" ~

        "int packInt2x16(i16vec2);" ~
        "uint packUint2x16(u16vec2);" ~
        "int64_t packInt4x16(i16vec4);" ~
        "uint64_t packUint4x16(u16vec4);" ~
        "i16vec2 unpackInt2x16(int);" ~
        "u16vec2 unpackUint2x16(uint);" ~
        "i16vec4 unpackInt4x16(int64_t);" ~
        "u16vec4 unpackUint4x16(uint64_t);" ~

        "bvec2 lessThan(i16vec2, i16vec2);" ~
        "bvec3 lessThan(i16vec3, i16vec3);" ~
        "bvec4 lessThan(i16vec4, i16vec4);" ~
        "bvec2 lessThan(u16vec2, u16vec2);" ~
        "bvec3 lessThan(u16vec3, u16vec3);" ~
        "bvec4 lessThan(u16vec4, u16vec4);" ~

        "bvec2 lessThanEqual(i16vec2, i16vec2);" ~
        "bvec3 lessThanEqual(i16vec3, i16vec3);" ~
        "bvec4 lessThanEqual(i16vec4, i16vec4);" ~
        "bvec2 lessThanEqual(u16vec2, u16vec2);" ~
        "bvec3 lessThanEqual(u16vec3, u16vec3);" ~
        "bvec4 lessThanEqual(u16vec4, u16vec4);" ~

        "bvec2 greaterThan(i16vec2, i16vec2);" ~
        "bvec3 greaterThan(i16vec3, i16vec3);" ~
        "bvec4 greaterThan(i16vec4, i16vec4);" ~
        "bvec2 greaterThan(u16vec2, u16vec2);" ~
        "bvec3 greaterThan(u16vec3, u16vec3);" ~
        "bvec4 greaterThan(u16vec4, u16vec4);" ~

        "bvec2 greaterThanEqual(i16vec2, i16vec2);" ~
        "bvec3 greaterThanEqual(i16vec3, i16vec3);" ~
        "bvec4 greaterThanEqual(i16vec4, i16vec4);" ~
        "bvec2 greaterThanEqual(u16vec2, u16vec2);" ~
        "bvec3 greaterThanEqual(u16vec3, u16vec3);" ~
        "bvec4 greaterThanEqual(u16vec4, u16vec4);" ~

        "bvec2 equal(i16vec2, i16vec2);" ~
        "bvec3 equal(i16vec3, i16vec3);" ~
        "bvec4 equal(i16vec4, i16vec4);" ~
        "bvec2 equal(u16vec2, u16vec2);" ~
        "bvec3 equal(u16vec3, u16vec3);" ~
        "bvec4 equal(u16vec4, u16vec4);" ~

        "bvec2 notEqual(i16vec2, i16vec2);" ~
        "bvec3 notEqual(i16vec3, i16vec3);" ~
        "bvec4 notEqual(i16vec4, i16vec4);" ~
        "bvec2 notEqual(u16vec2, u16vec2);" ~
        "bvec3 notEqual(u16vec3, u16vec3);" ~
        "bvec4 notEqual(u16vec4, u16vec4);" ~

        "int16_t bitfieldExtract(int16_t, int16_t, int16_t);" ~
        "i16vec2 bitfieldExtract(i16vec2, int16_t, int16_t);" ~
        "i16vec3 bitfieldExtract(i16vec3, int16_t, int16_t);" ~
        "i16vec4 bitfieldExtract(i16vec4, int16_t, int16_t);" ~

        "uint16_t bitfieldExtract(uint16_t, int16_t, int16_t);" ~
        "u16vec2 bitfieldExtract(u16vec2, int16_t, int16_t);" ~
        "u16vec3 bitfieldExtract(u16vec3, int16_t, int16_t);" ~
        "u16vec4 bitfieldExtract(u16vec4, int16_t, int16_t);" ~

        "int16_t bitfieldInsert(int16_t base, int16_t, int16_t, int16_t);" ~
        "i16vec2 bitfieldInsert(i16vec2 base, i16vec2, int16_t, int16_t);" ~
        "i16vec3 bitfieldInsert(i16vec3 base, i16vec3, int16_t, int16_t);" ~
        "i16vec4 bitfieldInsert(i16vec4 base, i16vec4, int16_t, int16_t);" ~

        "uint16_t bitfieldInsert(uint16_t base, uint16_t, int16_t, int16_t);" ~
        "u16vec2 bitfieldInsert(u16vec2 base, u16vec2, int16_t, int16_t);" ~
        "u16vec3 bitfieldInsert(u16vec3 base, u16vec3, int16_t, int16_t);" ~
        "u16vec4 bitfieldInsert(u16vec4 base, u16vec4, int16_t, int16_t);" ~

        "int16_t bitCount(int16_t);" ~
        "i16vec2 bitCount(i16vec2);" ~
        "i16vec3 bitCount(i16vec3);" ~
        "i16vec4 bitCount(i16vec4);" ~

        "int16_t bitCount(uint16_t);" ~
        "i16vec2 bitCount(u16vec2);" ~
        "i16vec3 bitCount(u16vec3);" ~
        "i16vec4 bitCount(u16vec4);" ~

        "int16_t findLSB(int16_t);" ~
        "i16vec2 findLSB(i16vec2);" ~
        "i16vec3 findLSB(i16vec3);" ~
        "i16vec4 findLSB(i16vec4);" ~

        "int16_t findLSB(uint16_t);" ~
        "i16vec2 findLSB(u16vec2);" ~
        "i16vec3 findLSB(u16vec3);" ~
        "i16vec4 findLSB(u16vec4);" ~

        "int16_t findMSB(int16_t);" ~
        "i16vec2 findMSB(i16vec2);" ~
        "i16vec3 findMSB(i16vec3);" ~
        "i16vec4 findMSB(i16vec4);" ~

        "int16_t findMSB(uint16_t);" ~
        "i16vec2 findMSB(u16vec2);" ~
        "i16vec3 findMSB(u16vec3);" ~
        "i16vec4 findMSB(u16vec4);" ~

        "int16_t pack16(i8vec2);" ~
        "uint16_t pack16(u8vec2);" ~
        "int32_t pack32(i8vec4);" ~
        "uint32_t pack32(u8vec4);" ~
        "int32_t pack32(i16vec2);" ~
        "uint32_t pack32(u16vec2);" ~
        "int64_t pack64(i16vec4);" ~
        "uint64_t pack64(u16vec4);" ~
        "int64_t pack64(i32vec2);" ~
        "uint64_t pack64(u32vec2);" ~

        "i8vec2 unpack8(int16_t);" ~
        "u8vec2 unpack8(uint16_t);" ~
        "i8vec4 unpack8(int32_t);" ~
        "u8vec4 unpack8(uint32_t);" ~
        "i16vec2 unpack16(int32_t);" ~
        "u16vec2 unpack16(uint32_t);" ~
        "i16vec4 unpack16(int64_t);" ~
        "u16vec4 unpack16(uint64_t);" ~
        "i32vec2 unpack32(int64_t);" ~
        "u32vec2 unpack32(uint64_t);" ~

        "int8_t expectEXT(int8_t, int8_t);" ~
        "i8vec2 expectEXT(i8vec2, i8vec2);" ~
        "i8vec3 expectEXT(i8vec3, i8vec3);" ~
        "i8vec4 expectEXT(i8vec4, i8vec4);" ~

        "uint8_t expectEXT(uint8_t, uint8_t);" ~
        "u8vec2 expectEXT(u8vec2, u8vec2);" ~
        "u8vec3 expectEXT(u8vec3, u8vec3);" ~
        "u8vec4 expectEXT(u8vec4, u8vec4);" ~

        "int16_t expectEXT(int16_t, int16_t);" ~
        "i16vec2 expectEXT(i16vec2, i16vec2);" ~
        "i16vec3 expectEXT(i16vec3, i16vec3);" ~
        "i16vec4 expectEXT(i16vec4, i16vec4);" ~

        "uint16_t expectEXT(uint16_t, uint16_t);" ~
        "u16vec2 expectEXT(u16vec2, u16vec2);" ~
        "u16vec3 expectEXT(u16vec3, u16vec3);" ~
        "u16vec4 expectEXT(u16vec4, u16vec4);" ~

        "int64_t expectEXT(int64_t, int64_t);" ~
        "i64vec2 expectEXT(i64vec2, i64vec2);" ~
        "i64vec3 expectEXT(i64vec3, i64vec3);" ~
        "i64vec4 expectEXT(i64vec4, i64vec4);" ~

        "uint64_t expectEXT(uint64_t, uint64_t);" ~
        "u64vec2 expectEXT(u64vec2, u64vec2);" ~
        "u64vec3 expectEXT(u64vec3, u64vec3);" ~
        "u64vec4 expectEXT(u64vec4, u64vec4);" ~
        "\n"
      );
    }

    if ((profile == EProfile(ES_PROFILE: 1) && version_ >= 300) ||
      (profile != EProfile(ES_PROFILE: 1) && version_ >= 130)) {
      commonBuiltins ~= (
        "float texture(sampler2DArrayShadow, vec4, float);" ~
        "float texture(samplerCubeArrayShadow, vec4, float, float);" ~
        "float textureLod(sampler2DArrayShadow, vec4, float);" ~
        "float textureLod(samplerCubeShadow, vec4, float);" ~
        "float textureLod(samplerCubeArrayShadow, vec4, float, float);" ~
        "float textureLodOffset(sampler2DArrayShadow, vec4, float, ivec2);" ~
        "float textureOffset(sampler2DArrayShadow, vec4, ivec2, float);" ~
        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 450) {
      stageBuiltins.STAGE_FRAGMENT ~= derivativesAndControl64bits;
      stageBuiltins.STAGE_FRAGMENT ~= (
        "float64_t interpolateAtCentroid(float64_t);" ~
        "f64vec2 interpolateAtCentroid(f64vec2);" ~
        "f64vec3 interpolateAtCentroid(f64vec3);" ~
        "f64vec4 interpolateAtCentroid(f64vec4);" ~

        "float64_t interpolateAtSample(float64_t, int);" ~
        "f64vec2 interpolateAtSample(f64vec2, int);" ~
        "f64vec3 interpolateAtSample(f64vec3, int);" ~
        "f64vec4 interpolateAtSample(f64vec4, int);" ~

        "float64_t interpolateAtOffset(float64_t, f64vec2);" ~
        "f64vec2 interpolateAtOffset(f64vec2, f64vec2);" ~
        "f64vec3 interpolateAtOffset(f64vec3, f64vec2);" ~
        "f64vec4 interpolateAtOffset(f64vec4, f64vec2);" ~

        "\n"
      );
    }

    if ((profile == EProfile(ES_PROFILE: 1) && version_ >= 310) ||
      (profile != EProfile(ES_PROFILE: 1) && version_ >= 140)) {
      commonBuiltins ~= (
        "void assumeEXT(bool);" ~

        "bool expectEXT(bool, bool);" ~
        "bvec2 expectEXT(bvec2, bvec2);" ~
        "bvec3 expectEXT(bvec3, bvec3);" ~
        "bvec4 expectEXT(bvec4, bvec4);" ~

        "int expectEXT(int, int);" ~
        "ivec2 expectEXT(ivec2, ivec2);" ~
        "ivec3 expectEXT(ivec3, ivec3);" ~
        "ivec4 expectEXT(ivec4, ivec4);" ~

        "uint expectEXT(uint, uint);" ~
        "uvec2 expectEXT(uvec2, uvec2);" ~
        "uvec3 expectEXT(uvec3, uvec3);" ~
        "uvec4 expectEXT(uvec4, uvec4);" ~
        "\n"
      );
    }

    if ((profile == EProfile(ES_PROFILE: 1) && version_ >= 310) ||
      (profile != EProfile(ES_PROFILE: 1) && version_ >= 140)) {
      commonBuiltins ~= (
        "vec4 textureWeightedQCOM(sampler2D, vec2, sampler2DArray);" ~
        "vec4 textureWeightedQCOM(sampler2D, vec2, sampler1DArray);" ~
        "vec4 textureBoxFilterQCOM(sampler2D, vec2, vec2);" ~
        "vec4 textureBlockMatchSADQCOM(sampler2D, uvec2, sampler2D, uvec2, uvec2);" ~
        "vec4 textureBlockMatchSSDQCOM(sampler2D, uvec2, sampler2D, uvec2, uvec2);" ~

        "vec4 textureBlockMatchWindowSSDQCOM(sampler2D, uvec2, sampler2D, uvec2, uvec2);" ~
        "vec4 textureBlockMatchWindowSADQCOM(sampler2D, uvec2, sampler2D, uvec2, uvec2);" ~
        "vec4 textureBlockMatchGatherSSDQCOM(sampler2D, uvec2, sampler2D, uvec2, uvec2);" ~
        "vec4 textureBlockMatchGatherSADQCOM(sampler2D, uvec2, sampler2D, uvec2, uvec2);" ~
        "\n"
      );
    }

    if (spvVersion.vulkan == 0 && IncludeLegacy(version_, profile, spvVersion))
      stageBuiltins.STAGE_VERTEX ~= "vec4 ftransform();";
  
    {
      void insert(string str) {
        if (version_ == 100)
          stageBuiltins.STAGE_VERTEX.insert = str;
        else
          commonBuiltins.insert = str;
      }
      if ((profile == EProfile(ES_PROFILE: 1) && version_ == 100) ||
        (profile == EProfile(CORE_PROFILE: 1) && version_ < 420) ||
        profile == EProfile(COMPATIBILITY_PROFILE: 1) ||
        profile == EProfile(NO_PROFILE: 1)) {
        if (spvVersion.spv == 0) {
          insert(
            "vec4 texture2DLod(sampler2D, vec2, float);" ~
            "vec4 texture2DProjLod(sampler2D, vec3, float);" ~
            "vec4 texture2DProjLod(sampler2D, vec4, float);" ~
            "vec4 texture3DLod(sampler3D, vec3, float);" ~
            "vec4 texture3DProjLod(sampler3D, vec4, float);" ~
            "vec4 textureCubeLod(samplerCube, vec3, float);" ~

            "\n"
          );
        }
      }
      if ((profile == EProfile(CORE_PROFILE: 1) && version_ < 420) ||
        profile == EProfile(COMPATIBILITY_PROFILE: 1) ||
        profile == EProfile(NO_PROFILE: 1)) {
        if (spvVersion.spv == 0) {
          insert(
            "vec4 texture1DLod(sampler1D, float, float);" ~
            "vec4 texture1DProjLod(sampler1D, vec2, float);" ~
            "vec4 texture1DProjLod(sampler1D, vec4, float);" ~
            "vec4 shadow1DLod(sampler1DShadow, vec3, float);" ~
            "vec4 shadow2DLod(sampler2DShadow, vec3, float);" ~
            "vec4 shadow1DProjLod(sampler1DShadow, vec4, float);" ~
            "vec4 shadow2DProjLod(sampler2DShadow, vec4, float);" ~

            "vec4 texture1DGradARB(sampler1D, float, float, float);" ~
            "vec4 texture1DProjGradARB(sampler1D, vec2, float, float);" ~
            "vec4 texture1DProjGradARB(sampler1D, vec4, float, float);" ~
            "vec4 texture2DGradARB(sampler2D, vec2, vec2, vec2);" ~
            "vec4 texture2DProjGradARB(sampler2D, vec3, vec2, vec2);" ~
            "vec4 texture2DProjGradARB(sampler2D, vec4, vec2, vec2);" ~
            "vec4 texture3DGradARB(sampler3D, vec3, vec3, vec3);" ~
            "vec4 texture3DProjGradARB(sampler3D, vec4, vec3, vec3);" ~
            "vec4 textureCubeGradARB(samplerCube, vec3, vec3, vec3);" ~
            "vec4 shadow1DGradARB(sampler1DShadow, vec3, float, float);" ~
            "vec4 shadow1DProjGradARB(sampler1DShadow, vec4, float, float);" ~
            "vec4 shadow2DGradARB(sampler2DShadow, vec3, vec2, vec2);" ~
            "vec4 shadow2DProjGradARB(sampler2DShadow, vec4, vec2, vec2);" ~
            "vec4 texture2DRectGradARB(sampler2DRect, vec2, vec2, vec2);" ~
            "vec4 texture2DRectProjGradARB(sampler2DRect, vec3, vec2, vec2);" ~
            "vec4 texture2DRectProjGradARB(sampler2DRect, vec4, vec2, vec2);" ~
            "vec4 shadow2DRectGradARB(sampler2DRectShadow, vec3, vec2, vec2);" ~
            "vec4 shadow2DRectProjGradARB(sampler2DRectShadow, vec4, vec2, vec2);" ~

            "\n"
          );
        }
      }
    }

    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 150) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 310)) {
      if (profile != EProfile(ES_PROFILE: 1) && version_ >= 150) {
        stageBuiltins.STAGE_GEOMETRY ~= (
          "void EmitStreamVertex(int);" ~
          "void EndStreamPrimitive(int);"
        );
      }
      stageBuiltins.STAGE_GEOMETRY ~= (
        "void EmitVertex();" ~
        "void EndPrimitive();" ~
        "\n"
      );
    }

    bool esBarrier = (profile == EProfile(ES_PROFILE: 1) && version_ >= 310);
    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 150) || esBarrier)
      stageBuiltins.STAGE_TESSCONTROL ~= (
        "void barrier();"
      );
    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 420) || esBarrier)
      stageBuiltins.STAGE_COMPUTE ~= (
        "void barrier();"
      );
    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 450) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 320)) {
      stageBuiltins.STAGE_MESH ~= (
        "void barrier();"
      );
      stageBuiltins.STAGE_TASK ~= (
        "void barrier();"
      );
    }
    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 130) || esBarrier)
      commonBuiltins ~= (
        "void memoryBarrier();"
      );
    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 420) || esBarrier) {
      commonBuiltins ~= (
        "void memoryBarrierBuffer();"
      );
      stageBuiltins.STAGE_COMPUTE ~= (
        "void memoryBarrierShared();" ~
        "void groupMemoryBarrier();"
      );
    }
    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 420) || esBarrier) {
      if (spvVersion.vulkan == 0 || spvVersion.vulkanRelaxed) {
        commonBuiltins ~= "void memoryBarrierAtomicCounter();";
      }
      commonBuiltins ~= "void memoryBarrierImage();";
    }
    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 450) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 320)) {
      stageBuiltins.STAGE_MESH ~= (
        "void memoryBarrierShared();" ~
        "void groupMemoryBarrier();"
      );
      stageBuiltins.STAGE_TASK ~= (
        "void memoryBarrierShared();" ~
        "void groupMemoryBarrier();"
      );
    }

    commonBuiltins ~= (
      "void controlBarrier(int, int, int, int);\n" ~
      "void memoryBarrier(int, int, int);\n" ~
      "void debugPrintfEXT();\n"
    );

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 450) {
      stageBuiltins.STAGE_COMPUTE ~= (
        "void coopMatLoadNV(out fcoopmatNV m, volatile coherent nontemporal float16_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out fcoopmatNV m, volatile coherent nontemporal float[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out fcoopmatNV m, volatile coherent nontemporal uint8_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out fcoopmatNV m, volatile coherent nontemporal uint16_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out fcoopmatNV m, volatile coherent nontemporal uint[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out fcoopmatNV m, volatile coherent nontemporal uint64_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out fcoopmatNV m, volatile coherent nontemporal uvec2[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out fcoopmatNV m, volatile coherent nontemporal uvec4[] buf, uint element, uint stride, bool colMajor);\n" ~

        "void coopMatStoreNV(fcoopmatNV m, volatile coherent nontemporal float16_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(fcoopmatNV m, volatile coherent nontemporal float[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(fcoopmatNV m, volatile coherent nontemporal float64_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(fcoopmatNV m, volatile coherent nontemporal uint8_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(fcoopmatNV m, volatile coherent nontemporal uint16_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(fcoopmatNV m, volatile coherent nontemporal uint[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(fcoopmatNV m, volatile coherent nontemporal uint64_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(fcoopmatNV m, volatile coherent nontemporal uvec2[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(fcoopmatNV m, volatile coherent nontemporal uvec4[] buf, uint element, uint stride, bool colMajor);\n" ~

        "fcoopmatNV coopMatMulAddNV(fcoopmatNV A, fcoopmatNV B, fcoopmatNV C);\n" ~
        "void coopMatLoadNV(out icoopmatNV m, volatile coherent nontemporal int8_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out icoopmatNV m, volatile coherent nontemporal int16_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out icoopmatNV m, volatile coherent nontemporal int[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out icoopmatNV m, volatile coherent nontemporal int64_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out icoopmatNV m, volatile coherent nontemporal ivec2[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out icoopmatNV m, volatile coherent nontemporal ivec4[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out icoopmatNV m, volatile coherent nontemporal uint8_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out icoopmatNV m, volatile coherent nontemporal uint16_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out icoopmatNV m, volatile coherent nontemporal uint[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out icoopmatNV m, volatile coherent nontemporal uint64_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out icoopmatNV m, volatile coherent nontemporal uvec2[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out icoopmatNV m, volatile coherent nontemporal uvec4[] buf, uint element, uint stride, bool colMajor);\n" ~

        "void coopMatLoadNV(out ucoopmatNV m, volatile coherent nontemporal int8_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out ucoopmatNV m, volatile coherent nontemporal int16_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out ucoopmatNV m, volatile coherent nontemporal int[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out ucoopmatNV m, volatile coherent nontemporal int64_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out ucoopmatNV m, volatile coherent nontemporal ivec2[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out ucoopmatNV m, volatile coherent nontemporal ivec4[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out ucoopmatNV m, volatile coherent nontemporal uint8_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out ucoopmatNV m, volatile coherent nontemporal uint16_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out ucoopmatNV m, volatile coherent nontemporal uint[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out ucoopmatNV m, volatile coherent nontemporal uint64_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out ucoopmatNV m, volatile coherent nontemporal uvec2[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatLoadNV(out ucoopmatNV m, volatile coherent nontemporal uvec4[] buf, uint element, uint stride, bool colMajor);\n" ~

        "void coopMatStoreNV(icoopmatNV m, volatile coherent nontemporal int8_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(icoopmatNV m, volatile coherent nontemporal int16_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(icoopmatNV m, volatile coherent nontemporal int[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(icoopmatNV m, volatile coherent nontemporal int64_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(icoopmatNV m, volatile coherent nontemporal ivec2[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(icoopmatNV m, volatile coherent nontemporal ivec4[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(icoopmatNV m, volatile coherent nontemporal uint8_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(icoopmatNV m, volatile coherent nontemporal uint16_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(icoopmatNV m, volatile coherent nontemporal uint[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(icoopmatNV m, volatile coherent nontemporal uint64_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(icoopmatNV m, volatile coherent nontemporal uvec2[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(icoopmatNV m, volatile coherent nontemporal uvec4[] buf, uint element, uint stride, bool colMajor);\n" ~

        "void coopMatStoreNV(ucoopmatNV m, volatile coherent nontemporal int8_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(ucoopmatNV m, volatile coherent nontemporal int16_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(ucoopmatNV m, volatile coherent nontemporal int[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(ucoopmatNV m, volatile coherent nontemporal int64_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(ucoopmatNV m, volatile coherent nontemporal ivec2[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(ucoopmatNV m, volatile coherent nontemporal ivec4[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(ucoopmatNV m, volatile coherent nontemporal uint8_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(ucoopmatNV m, volatile coherent nontemporal uint16_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(ucoopmatNV m, volatile coherent nontemporal uint[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(ucoopmatNV m, volatile coherent nontemporal uint64_t[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(ucoopmatNV m, volatile coherent nontemporal uvec2[] buf, uint element, uint stride, bool colMajor);\n" ~
        "void coopMatStoreNV(ucoopmatNV m, volatile coherent nontemporal uvec4[] buf, uint element, uint stride, bool colMajor);\n" ~

        "icoopmatNV coopMatMulAddNV(icoopmatNV A, icoopmatNV B, icoopmatNV C);\n" ~
        "ucoopmatNV coopMatMulAddNV(ucoopmatNV A, ucoopmatNV B, ucoopmatNV C);\n"
      );

      DList!string cooperativeMatrixFuncs;

      {
        enum string[] allTypes = [
          "float", "vec2", "vec4",
          "float16_t", "f16vec2", "f16vec4",
          "bfloat16_t", "bf16vec2", "bf16vec4",
          "floate5m2_t", "fe5m2vec2", "fe5m2vec4",
          "floate4m3_t", "fe4m3vec2", "fe4m3vec4",
          "double", "dvec2", "dvec4",
          "int8_t", "i8vec2", "i8vec4",
          "int16_t", "i16vec2", "i16vec4",
          "int", "ivec2", "ivec4",
          "int64_t", "i64vec2", "i64vec4",
          "uint8_t", "u8vec2", "u8vec4",
          "uint16_t", "u16vec2", "u16vec4",
          "uint", "uvec2", "uvec4",
          "uint64_t", "u64vec2", "u64vec4",
        ];

        foreach (elemTy; ["uint", "uint64_t"]) {
          foreach (t; allTypes) {
            cooperativeMatrixFuncs ~= i"void coopMatLoad(out coopmat m, volatile coherent nontemporal $(t)[] buf, $(elemTy) element, uint stride, int matrixLayout);\n".text;
            cooperativeMatrixFuncs ~= i"void coopMatStore(coopmat m, volatile coherent nontemporal $(t)[] buf, $(elemTy) element, uint stride, int matrixLayout);\n".text;
          }
          cooperativeMatrixFuncs ~= i"void coopMatLoadTensorNV(inout coopmat m, volatile coherent nontemporal uint8_t[] buf, $(elemTy) element, tensorLayoutNV t);\n".text;
          cooperativeMatrixFuncs ~= i"void coopMatLoadTensorNV(inout coopmat m, volatile coherent nontemporal uint8_t[] buf, $(elemTy) element, tensorLayoutNV t, tensorViewNV v);\n".text;
          cooperativeMatrixFuncs ~= i"void coopMatLoadTensorNV(inout coopmat m, volatile coherent nontemporal uint8_t[] buf, $(elemTy) element, tensorLayoutNV t, __function f);\n".text;
          cooperativeMatrixFuncs ~= i"void coopMatLoadTensorNV(inout coopmat m, volatile coherent nontemporal uint8_t[] buf, $(elemTy) element, tensorLayoutNV t, tensorViewNV v, __function f);\n".text;
          cooperativeMatrixFuncs ~= i"void coopMatStoreTensorNV(coopmat m, volatile coherent nontemporal uint8_t[] buf, $(elemTy) element, tensorLayoutNV t);\n".text;
          cooperativeMatrixFuncs ~= i"void coopMatStoreTensorNV(coopmat m, volatile coherent nontemporal uint8_t[] buf, $(elemTy) element, tensorLayoutNV t, tensorViewNV v);\n".text;
        }
      }

      cooperativeMatrixFuncs ~= (
        "coopmat coopMatMulAdd(coopmat A, coopmat B, coopmat C);\n" ~
        "coopmat coopMatMulAdd(coopmat A, coopmat B, coopmat C, int matrixOperands);\n"
      );

      commonBuiltins ~= cooperativeMatrixFuncs[];

      commonBuiltins ~= (
        "const int gl_MatrixUseA = 0;\n" ~
        "const int gl_MatrixUseB = 1;\n" ~
        "const int gl_MatrixUseAccumulator = 2;\n" ~
        "const int gl_MatrixOperandsSaturatingAccumulation = 0x10;\n" ~
        "const int gl_CooperativeMatrixLayoutRowMajor = 0;\n" ~
        "const int gl_CooperativeMatrixLayoutColumnMajor = 1;\n" ~
        "const int gl_CooperativeMatrixLayoutRowBlockedInterleavedARM = 4202;\n" ~
        "const int gl_CooperativeMatrixLayoutColumnBlockedInterleavedARM = 4203;\n" ~
        "\n" ~

        "void coopMatTransposeNV(out coopmat, coopmat);" ~
        "void coopMatReduceNV(out coopmat, coopmat, int, __function);" ~
        "void coopMatPerElementNV();" ~

        "const int gl_CooperativeMatrixReduceRowNV = 0x1;\n" ~
        "const int gl_CooperativeMatrixReduceColumnNV = 0x2;\n" ~
        "const int gl_CooperativeMatrixReduceRowAndColumnNV = 0x3;\n" ~
        "const int gl_CooperativeMatrixReduce2x2NV = 0x4;\n" ~
        "\n" ~

        "const int gl_CooperativeMatrixClampModeUndefinedNV = 0x0;\n" ~
        "const int gl_CooperativeMatrixClampModeConstantNV = 0x1;\n" ~
        "const int gl_CooperativeMatrixClampModeClampToEdgeNV = 0x2;\n" ~
        "const int gl_CooperativeMatrixClampModeRepeatNV = 0x3;\n" ~
        "const int gl_CooperativeMatrixClampModeMirrorRepeatNV = 0x4;\n" ~
        "\n"
      );

      {
        DList!string coopMatConvFuncs;

        enum string[] eltTypes = [
          "uint32_t", "uint", "int32_t", "int", "float32_t", "float", "float16_t"
        ];

        foreach (srcEltTy; eltTypes) {
          foreach (dstEltTy; eltTypes) {
            coopMatConvFuncs ~= i"void bitcastQCOM($(srcEltTy) SrcArr[], $(dstEltTy) DstArr[]);\n".text;
          }
        }
        coopMatConvFuncs ~= "\n";

        foreach (
          eltTy; ["float32_t", "float16_t", "int8_t", "uint8_t", "uint32_t", "uint", "int32_t", "int"]
        ) {
          coopMatConvFuncs ~= i"void vectorToCoopmatQCOM($(eltTy) SrcVec[], coopmat CM);\n".text;
          coopMatConvFuncs ~= i"void coopmatToVectorQCOM(coopmat CM, $(eltTy) Dstvec[]);\n".text;
        }

        foreach (eltTy; ["uint32_t", "uint", "int32_t", "int",
          "float32_t", "float", "float16_t"]
        ) {
          coopMatConvFuncs ~= i"void extractSubArrayQCOM($(eltTy) arr[], uint index, $(eltTy) subarr[]);\n".text;
        }

        commonBuiltins ~= coopMatConvFuncs[];
      }

      commonBuiltins ~= (
        "tensorLayoutNV createTensorLayoutNV(uint Dim);\n" ~
        "tensorLayoutNV createTensorLayoutNV(uint Dim, uint Mode);\n" ~

        "tensorLayoutNV setTensorLayoutBlockSizeNV(tensorLayoutNV t, uint blockSize0);\n" ~
        "tensorLayoutNV setTensorLayoutBlockSizeNV(tensorLayoutNV t, uint blockSize0, uint blockSize1);\n" ~
        "tensorLayoutNV setTensorLayoutBlockSizeNV(tensorLayoutNV t, uint blockSize0, uint blockSize1, uint blockSize2);\n" ~
        "tensorLayoutNV setTensorLayoutBlockSizeNV(tensorLayoutNV t, uint blockSize0, uint blockSize1, uint blockSize2, uint blockSize3);\n" ~
        "tensorLayoutNV setTensorLayoutBlockSizeNV(tensorLayoutNV t, uint blockSize0, uint blockSize1, uint blockSize2, uint blockSize3, uint blockSize4);\n" ~

        "tensorLayoutNV setTensorLayoutDimensionNV(tensorLayoutNV t, uint dim0);\n" ~
        "tensorLayoutNV setTensorLayoutDimensionNV(tensorLayoutNV t, uint dim0, uint dim1);\n" ~
        "tensorLayoutNV setTensorLayoutDimensionNV(tensorLayoutNV t, uint dim0, uint dim1, uint dim2);\n" ~
        "tensorLayoutNV setTensorLayoutDimensionNV(tensorLayoutNV t, uint dim0, uint dim1, uint dim2, uint dim3);\n" ~
        "tensorLayoutNV setTensorLayoutDimensionNV(tensorLayoutNV t, uint dim0, uint dim1, uint dim2, uint dim3, uint dim4);\n" ~

        "tensorLayoutNV setTensorLayoutStrideNV(tensorLayoutNV t, uint stride0);\n" ~
        "tensorLayoutNV setTensorLayoutStrideNV(tensorLayoutNV t, uint stride0, uint stride1);\n" ~
        "tensorLayoutNV setTensorLayoutStrideNV(tensorLayoutNV t, uint stride0, uint stride1, uint stride2);\n" ~
        "tensorLayoutNV setTensorLayoutStrideNV(tensorLayoutNV t, uint stride0, uint stride1, uint stride2, uint stride3);\n" ~
        "tensorLayoutNV setTensorLayoutStrideNV(tensorLayoutNV t, uint stride0, uint stride1, uint stride2, uint stride3, uint stride4);\n" ~

        "tensorLayoutNV sliceTensorLayoutNV(tensorLayoutNV t, uint offset0, uint span0);\n" ~
        "tensorLayoutNV sliceTensorLayoutNV(tensorLayoutNV t, uint offset0, uint span0, uint offset1, uint span1);\n" ~
        "tensorLayoutNV sliceTensorLayoutNV(tensorLayoutNV t, uint offset0, uint span0, uint offset1, uint span1, uint offset2, uint span2);\n" ~
        "tensorLayoutNV sliceTensorLayoutNV(tensorLayoutNV t, uint offset0, uint span0, uint offset1, uint span1, uint offset2, uint span2, uint offset3, uint span3);\n" ~
        "tensorLayoutNV sliceTensorLayoutNV(tensorLayoutNV t, uint offset0, uint span0, uint offset1, uint span1, uint offset2, uint span2, uint offset3, uint span3, uint offset4, uint span4);\n" ~

        "tensorLayoutNV setTensorLayoutClampValueNV(tensorLayoutNV t, uint value);\n" ~

        "tensorViewNV createTensorViewNV(uint Dim);\n" ~
        "tensorViewNV createTensorViewNV(uint Dim, bool HasDimensions);\n" ~
        "tensorViewNV createTensorViewNV(uint Dim, bool HasDimensions, uint p0);\n" ~
        "tensorViewNV createTensorViewNV(uint Dim, bool HasDimensions, uint p0, uint p1);\n" ~
        "tensorViewNV createTensorViewNV(uint Dim, bool HasDimensions, uint p0, uint p1, uint p2);\n" ~
        "tensorViewNV createTensorViewNV(uint Dim, bool HasDimensions, uint p0, uint p1, uint p2, uint p3);\n" ~
        "tensorViewNV createTensorViewNV(uint Dim, bool HasDimensions, uint p0, uint p1, uint p2, uint p3, uint p4);\n" ~

        "tensorViewNV setTensorViewDimensionsNV(tensorViewNV v, uint dim0);\n" ~
        "tensorViewNV setTensorViewDimensionsNV(tensorViewNV v, uint dim0, uint dim1);\n" ~
        "tensorViewNV setTensorViewDimensionsNV(tensorViewNV v, uint dim0, uint dim1, uint dim2);\n" ~
        "tensorViewNV setTensorViewDimensionsNV(tensorViewNV v, uint dim0, uint dim1, uint dim2, uint dim3);\n" ~
        "tensorViewNV setTensorViewDimensionsNV(tensorViewNV v, uint dim0, uint dim1, uint dim2, uint dim3, uint dim4);\n" ~

        "tensorViewNV setTensorViewStrideNV(tensorViewNV v, uint stride0);\n" ~
        "tensorViewNV setTensorViewStrideNV(tensorViewNV v, uint stride0, uint stride1);\n" ~
        "tensorViewNV setTensorViewStrideNV(tensorViewNV v, uint stride0, uint stride1, uint stride2);\n" ~
        "tensorViewNV setTensorViewStrideNV(tensorViewNV v, uint stride0, uint stride1, uint stride2, uint stride3);\n" ~
        "tensorViewNV setTensorViewStrideNV(tensorViewNV v, uint stride0, uint stride1, uint stride2, uint stride3, uint stride4);\n" ~

        "tensorViewNV setTensorViewClipNV(tensorViewNV v, uint clipRowOffset, uint clipRowSpan, uint clipColOffset, uint clipColSpan);\n" ~
        "\n"
      );

      enum string[] tensorDataTypesARM = [
        "bool",
        "int8_t", "int16_t", "int32_t", "int64_t",
        "uint8_t", "uint16_t", "uint32_t", "uint64_t",
        "float16_t", "float32_t", "float64_t"
      ];
      foreach (t; tensorDataTypesARM) {
        commonBuiltins ~= i"void tensorReadARM(readonly tensorARM t, uint coords[], out $(t) data, uint tensorOperands = 0U, ...);\n".text;
        commonBuiltins ~= i"void tensorWriteARM(writeonly tensorARM t, uint coords[], $(t) data, uint tensorOperands = 0U, ...);\n".text;
        commonBuiltins ~= i"void tensorReadARM(readonly tensorARM t, uint coords[], $(t) data[], uint tensorOperands = 0U, ...);\n".text;
        commonBuiltins ~= i"void tensorWriteARM(writeonly tensorARM t, uint coords[], $(t) data[], uint tensorOperands = 0U, ...);\n".text;
      }
      commonBuiltins ~= "uint tensorSizeARM(readonly writeonly tensorARM t, uint dim);\n";
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 450) {
      enum string[] basicTypes = [
        "int8_t",
        "int16_t",
        "int32_t",
        "int64_t",
        "uint8_t",
        "uint16_t",
        "uint32_t",
        "uint64_t",
        "float16_t",
        "float32_t",
        "float64_t"
      ];
      enum string[] coopVecOffsetTypes = [
        "uint",
        "uint64_t"
      ];
      foreach (offsetTy; coopVecOffsetTypes) {
        foreach (basicTy; basicTypes) {
          commonBuiltins ~=
            "void coopVecMatMulNV(out coopvecNV result, coopvecNV v, int inputInterpretation, "
              ~ i"$(basicTy)[] matrix, $(offsetTy) matrixOffset, int matrixInterpretation, ".text
              ~ "uint M, uint K, int matrixLayout, bool transpose, uint matrixStride);\n";
          
          foreach (biasBasicType; basicTypes) {
            commonBuiltins ~= "void coopVecMatMulAddNV(out coopvecNV result, coopvecNV v, int inputInterpretation, "
              ~ i"$(basicTy)[] matrix, $(offsetTy) matrixOffset, int matrixInterpretation, ".text
              ~ i"$(biasBasicType)[] bias, $(offsetTy) biasOffset, int biasInterpretation, ".text
              ~ "uint M, uint K, int matrixLayout, bool transpose, uint matrixStride);\n";
          }

          commonBuiltins ~= "void coopVecOuterProductAccumulateNV(coopvecNV v1, coopvecNV v2, "
            ~ i"$(basicTy)[] buf, $(offsetTy) offset, uint stride, int matrixLayout, int matrixInterpretation);\n".text;

          commonBuiltins ~= "void coopVecReduceSumAccumulateNV(coopvecNV v, "
            ~ i"$(basicTy)[] buf, $(offsetTy) offset);\n".text;
        }
      }
      commonBuiltins ~= (
        "coopvecNV fma(coopvecNV, coopvecNV, coopvecNV);" ~
        "coopvecNV min(coopvecNV, coopvecNV);" ~
        "coopvecNV max(coopvecNV, coopvecNV);" ~
        "coopvecNV step(coopvecNV, coopvecNV);" ~
        "coopvecNV exp(coopvecNV);" ~            
        "coopvecNV log(coopvecNV);" ~            
        "coopvecNV tanh(coopvecNV);" ~            
        "coopvecNV atan(coopvecNV);" ~            
        "coopvecNV clamp(coopvecNV, coopvecNV, coopvecNV);" ~
        "\n"
      );

      enum string[] scalarAndVectorTypes = [
        "int8_t",
        "int16_t",
        "int32_t",
        "int64_t",
        "uint8_t",
        "uint16_t",
        "uint32_t",
        "uint64_t",
        "float16_t",
        "float32_t",
        "float64_t",
        "i8vec2",
        "i16vec2",
        "i32vec2",
        "i64vec2",
        "u8vec2",
        "u16vec2",
        "u32vec2",
        "u64vec2",
        "f16vec2",
        "f32vec2",
        "f64vec2",
        "i8vec3",
        "i16vec3",
        "i32vec3",
        "i64vec3",
        "u8vec3",
        "u16vec3",
        "u32vec3",
        "u64vec3",
        "f16vec3",
        "f32vec3",
        "f64vec3",
        "i8vec4",
        "i16vec4",
        "i32vec4",
        "i64vec4",
        "u8vec4",
        "u16vec4",
        "u32vec4",
        "u64vec4",
        "f16vec4",
        "f32vec4",
        "f64vec4"
      ];

      foreach (offsetTy; coopVecOffsetTypes) {
        foreach (scalarAndVectorTy; scalarAndVectorTypes) {
          commonBuiltins ~= "void coopVecLoadNV(out coopvecNV v, volatile coherent "
            ~ i"$(scalarAndVectorTy)[] buf, $(offsetTy) offset);".text;
          commonBuiltins ~= "void coopVecStoreNV(coopvecNV v, volatile coherent "
            ~ i"$(scalarAndVectorTy)[] buf, $(offsetTy) offset);".text;
        }
      }

      commonBuiltins ~= (
        "const int gl_CooperativeVectorMatrixLayoutRowMajorNV = 0;\n" ~
        "const int gl_CooperativeVectorMatrixLayoutColumnMajorNV = 1;\n" ~
        "const int gl_CooperativeVectorMatrixLayoutInferencingOptimalNV = 2;\n" ~
        "const int gl_CooperativeVectorMatrixLayoutTrainingOptimalNV = 3;\n" ~
        "\n" ~

        "const int gl_ComponentTypeFloat16NV = 0;\n" ~
        "const int gl_ComponentTypeFloat32NV = 1;\n" ~
        "const int gl_ComponentTypeFloat64NV = 2;\n" ~
        "const int gl_ComponentTypeSignedInt8NV = 3;\n" ~
        "const int gl_ComponentTypeSignedInt16NV = 4;\n" ~
        "const int gl_ComponentTypeSignedInt32NV = 5;\n" ~
        "const int gl_ComponentTypeSignedInt64NV = 6;\n" ~
        "const int gl_ComponentTypeUnsignedInt8NV = 7;\n" ~
        "const int gl_ComponentTypeUnsignedInt16NV = 8;\n" ~
        "const int gl_ComponentTypeUnsignedInt32NV = 9;\n" ~
        "const int gl_ComponentTypeUnsignedInt64NV = 10;\n" ~
        "const int gl_ComponentTypeSignedInt8PackedNV = 1000491000;\n" ~
        "const int gl_ComponentTypeUnsignedInt8PackedNV = 1000491001;\n" ~
        "const int gl_ComponentTypeFloatE4M3NV = 1000491002;\n" ~
        "const int gl_ComponentTypeFloatE5M2NV = 1000491003;\n" ~
        "\n"
      );
    }

    if (spvVersion.spv == 0 && (profile != EProfile(ES_PROFILE: 1) || version_ == 100)) {
      stageBuiltins.STAGE_FRAGMENT ~= (
        "vec4 texture2D(sampler2D, vec2, float);" ~
        "vec4 texture2DProj(sampler2D, vec3, float);" ~
        "vec4 texture2DProj(sampler2D, vec4, float);" ~
        "vec4 texture3D(sampler3D, vec3, float);" ~
        "vec4 texture3DProj(sampler3D, vec4, float);" ~
        "vec4 textureCube(samplerCube, vec3, float);" ~
        
        "\n"
      );
    }
    if (spvVersion.spv == 0 && (profile != EProfile(ES_PROFILE: 1) && version_ > 100)) {
      stageBuiltins.STAGE_FRAGMENT ~= (
        "vec4 texture1D(sampler1D, float, float);" ~
        "vec4 texture1DProj(sampler1D, vec2, float);" ~
        "vec4 texture1DProj(sampler1D, vec4, float);" ~
        "vec4 shadow1D(sampler1DShadow, vec3, float);" ~
        "vec4 shadow2D(sampler2DShadow, vec3, float);" ~
        "vec4 shadow1DProj(sampler1DShadow, vec4, float);" ~
        "vec4 shadow2DProj(sampler2DShadow, vec4, float);" ~

        "\n"
      );
    }
    if (spvVersion.spv == 0 && profile == EProfile(ES_PROFILE: 1)) {
      stageBuiltins.STAGE_FRAGMENT ~= (
        "vec4 texture2DLodEXT(sampler2D, vec2, float);" ~
        "vec4 texture2DProjLodEXT(sampler2D, vec3, float);" ~
        "vec4 texture2DProjLodEXT(sampler2D, vec4, float);" ~
        "vec4 textureCubeLodEXT(samplerCube, vec3, float);" ~

        "\n"
      );
    }

    if (spvVersion.vulkan > 0) {
      stageBuiltins.STAGE_FRAGMENT ~= (
        "lowp uint stencilAttachmentReadEXT();" ~
        "lowp uint stencilAttachmentReadEXT(int);" ~
        "highp float depthAttachmentReadEXT();" ~
        "highp float depthAttachmentReadEXT(int);" ~
        "\n" ~

        "vec4 colorAttachmentReadEXT(attachmentEXT);" ~
        "vec4 colorAttachmentReadEXT(attachmentEXT, int);" ~
        "ivec4 colorAttachmentReadEXT(iattachmentEXT);" ~
        "ivec4 colorAttachmentReadEXT(iattachmentEXT, int);" ~
        "uvec4 colorAttachmentReadEXT(uattachmentEXT);" ~
        "uvec4 colorAttachmentReadEXT(uattachmentEXT, int);" ~
        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 400) {
      stageBuiltins.STAGE_FRAGMENT ~= derivativeControls;
      stageBuiltins.STAGE_FRAGMENT ~= "\n";
    }

    if ((profile == EProfile(ES_PROFILE: 1) && version_ >= 310) ||
      (profile != EProfile(ES_PROFILE: 1) && version_ >= 150)) {
      stageBuiltins.STAGE_FRAGMENT ~= (
        "float interpolateAtCentroid(float);" ~
        "vec2 interpolateAtCentroid(vec2);" ~
        "vec3 interpolateAtCentroid(vec3);" ~
        "vec4 interpolateAtCentroid(vec4);" ~

        "float interpolateAtSample(float, int);" ~
        "vec2 interpolateAtSample(vec2, int);" ~
        "vec3 interpolateAtSample(vec3, int);" ~
        "vec4 interpolateAtSample(vec4, int);" ~

        "float interpolateAtOffset(float, vec2);" ~
        "vec2 interpolateAtOffset(vec2, vec2);" ~
        "vec3 interpolateAtOffset(vec3, vec2);" ~
        "vec4 interpolateAtOffset(vec4, vec2);" ~

        "\n"
      );
    }

    stageBuiltins.STAGE_FRAGMENT ~= (
      "void beginInvocationInterlockARB(void);" ~
      "void endInvocationInterlockARB(void);" ~

      "bool helperInvocationEXT();" ~
      "\n"
    );

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 450) {
      stageBuiltins.STAGE_FRAGMENT ~= (
        "float interpolateAtVertexAMD(float, uint);" ~
        "vec2 interpolateAtVertexAMD(vec2, uint);" ~
        "vec3 interpolateAtVertexAMD(vec3, uint);" ~
        "vec4 interpolateAtVertexAMD(vec4, uint);" ~

        "int interpolateAtVertexAMD(int, uint);" ~
        "ivec2 interpolateAtVertexAMD(ivec2, uint);" ~
        "ivec3 interpolateAtVertexAMD(ivec3, uint);" ~
        "ivec4 interpolateAtVertexAMD(ivec4, uint);" ~

        "uint interpolateAtVertexAMD(uint, uint);" ~
        "uvec2 interpolateAtVertexAMD(uvec2, uint);" ~
        "uvec3 interpolateAtVertexAMD(uvec3, uint);" ~
        "uvec4 interpolateAtVertexAMD(uvec4, uint);" ~

        "float16_t interpolateAtVertexAMD(float16_t, uint);" ~
        "f16vec2 interpolateAtVertexAMD(f16vec2, uint);" ~
        "f16vec3 interpolateAtVertexAMD(f16vec3, uint);" ~
        "f16vec4 interpolateAtVertexAMD(f16vec4, uint);" ~

        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 450) {
      stageBuiltins.STAGE_FRAGMENT ~= derivativesAndControl16bits;
      stageBuiltins.STAGE_FRAGMENT ~= "\n";

      stageBuiltins.STAGE_FRAGMENT ~= (
        "float16_t interpolateAtCentroid(float16_t);" ~
        "f16vec2 interpolateAtCentroid(f16vec2);" ~
        "f16vec3 interpolateAtCentroid(f16vec3);" ~
        "f16vec4 interpolateAtCentroid(f16vec4);" ~

        "float16_t interpolateAtSample(float16_t, int);" ~
        "f16vec2 interpolateAtSample(f16vec2, int);" ~
        "f16vec3 interpolateAtSample(f16vec3, int);" ~
        "f16vec4 interpolateAtSample(f16vec4, int);" ~

        "float16_t interpolateAtOffset(float16_t, f16vec2);" ~
        "f16vec2 interpolateAtOffset(f16vec2, f16vec2);" ~
        "f16vec3 interpolateAtOffset(f16vec3, f16vec2);" ~
        "f16vec4 interpolateAtOffset(f16vec4, f16vec2);" ~

        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 450) {
      commonBuiltins ~= (
        "uvec2 clock2x32ARB();" ~
        "uint64_t clockARB();" ~
        "uvec2 clockRealtime2x32EXT();" ~
        "uint64_t clockRealtimeEXT();" ~
        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 450 && spvVersion.vulkan > 0) {
      stageBuiltins.STAGE_FRAGMENT ~= (
        "uint fragmentMaskFetchAMD(subpassInputMS);" ~
        "uint fragmentMaskFetchAMD(isubpassInputMS);" ~
        "uint fragmentMaskFetchAMD(usubpassInputMS);" ~

        "vec4 fragmentFetchAMD(subpassInputMS, uint);" ~
        "ivec4 fragmentFetchAMD(isubpassInputMS, uint);" ~
        "uvec4 fragmentFetchAMD(usubpassInputMS, uint);" ~

        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 460) {
      commonBuiltins ~= (
        "void rayQueryInitializeEXT(rayQueryEXT, accelerationStructureEXT, uint, uint, vec3, float, vec3, float);" ~
        "void rayQueryTerminateEXT(rayQueryEXT);" ~
        "void rayQueryGenerateIntersectionEXT(rayQueryEXT, float);" ~
        "void rayQueryConfirmIntersectionEXT(rayQueryEXT);" ~
        "bool rayQueryProceedEXT(rayQueryEXT);" ~
        "uint rayQueryGetIntersectionTypeEXT(rayQueryEXT, bool);" ~
        "float rayQueryGetRayTMinEXT(rayQueryEXT);" ~
        "uint rayQueryGetRayFlagsEXT(rayQueryEXT);" ~
        "vec3 rayQueryGetWorldRayOriginEXT(rayQueryEXT);" ~
        "vec3 rayQueryGetWorldRayDirectionEXT(rayQueryEXT);" ~
        "float rayQueryGetIntersectionTEXT(rayQueryEXT, bool);" ~
        "int rayQueryGetIntersectionInstanceCustomIndexEXT(rayQueryEXT, bool);" ~
        "int rayQueryGetIntersectionInstanceIdEXT(rayQueryEXT, bool);" ~
        "uint rayQueryGetIntersectionInstanceShaderBindingTableRecordOffsetEXT(rayQueryEXT, bool);" ~
        "int rayQueryGetIntersectionGeometryIndexEXT(rayQueryEXT, bool);" ~
        "int rayQueryGetIntersectionPrimitiveIndexEXT(rayQueryEXT, bool);" ~
        "vec2 rayQueryGetIntersectionBarycentricsEXT(rayQueryEXT, bool);" ~
        "bool rayQueryGetIntersectionFrontFaceEXT(rayQueryEXT, bool);" ~
        "bool rayQueryGetIntersectionCandidateAABBOpaqueEXT(rayQueryEXT);" ~
        "vec3 rayQueryGetIntersectionObjectRayDirectionEXT(rayQueryEXT, bool);" ~
        "vec3 rayQueryGetIntersectionObjectRayOriginEXT(rayQueryEXT, bool);" ~
        "mat4x3 rayQueryGetIntersectionObjectToWorldEXT(rayQueryEXT, bool);" ~
        "mat4x3 rayQueryGetIntersectionWorldToObjectEXT(rayQueryEXT, bool);" ~
        "void rayQueryGetIntersectionTriangleVertexPositionsEXT(rayQueryEXT, bool, out vec3[3]);" ~
        "int rayQueryGetIntersectionClusterIdNV(rayQueryEXT, bool);" ~
        "vec3 rayQueryGetIntersectionSpherePositionNV(rayQueryEXT, bool);" ~
        "float rayQueryGetIntersectionSphereRadiusNV(rayQueryEXT, bool);" ~
        "float rayQueryGetIntersectionLSSHitValueNV(rayQueryEXT, bool);" ~
        "void rayQueryGetIntersectionLSSPositionsNV(rayQueryEXT, bool, out vec3[2]);" ~
        "void rayQueryGetIntersectionLSSRadiiNV(rayQueryEXT, bool, out float[2]);" ~
        "bool rayQueryIsSphereHitNV(rayQueryEXT, bool);" ~
        "bool rayQueryIsLSSHitNV(rayQueryEXT, bool);" ~
        "\n"
      );

      stageBuiltins.STAGE_RAYGEN ~= (
        "void traceNV(accelerationStructureNV,uint,uint,uint,uint,uint,vec3,float,vec3,float,int);" ~
        "void traceRayMotionNV(accelerationStructureNV,uint,uint,uint,uint,uint,vec3,float,vec3,float,float,int);" ~
        "void traceRayEXT(accelerationStructureEXT,uint,uint,uint,uint,uint,vec3,float,vec3,float,int);" ~
        "void executeCallableNV(uint, int);" ~
        "void executeCallableEXT(uint, int);" ~
        "void hitObjectTraceRayNV(hitObjectNV,accelerationStructureEXT,uint,uint,uint,uint,uint,vec3,float,vec3,float,int);" ~
        "void hitObjectTraceRayMotionNV(hitObjectNV,accelerationStructureEXT,uint,uint,uint,uint,uint,vec3,float,vec3,float,float,int);" ~
        "void hitObjectRecordHitNV(hitObjectNV,accelerationStructureEXT,int,int,int,uint,uint,uint,vec3,float,vec3,float,int);" ~
        "void hitObjectRecordHitMotionNV(hitObjectNV,accelerationStructureEXT,int,int,int,uint,uint,uint,vec3,float,vec3,float,float,int);" ~
        "void hitObjectRecordHitWithIndexNV(hitObjectNV, accelerationStructureEXT,int,int,int,uint,uint,vec3,float,vec3,float,int);" ~
        "void hitObjectRecordHitWithIndexMotionNV(hitObjectNV, accelerationStructureEXT,int,int,int,uint,uint,vec3,float,vec3,float,float,int);" ~
        "void hitObjectRecordMissNV(hitObjectNV,uint,vec3,float,vec3,float);" ~
        "void hitObjectRecordMissMotionNV(hitObjectNV,uint,vec3,float,vec3,float,float);" ~
        "void hitObjectRecordEmptyNV(hitObjectNV);" ~
        "void hitObjectExecuteShaderNV(hitObjectNV,int);" ~
        "bool hitObjectIsEmptyNV(hitObjectNV);" ~
        "bool hitObjectIsMissNV(hitObjectNV);" ~
        "bool hitObjectIsHitNV(hitObjectNV);" ~
        "float hitObjectGetRayTMinNV(hitObjectNV);" ~
        "float hitObjectGetRayTMaxNV(hitObjectNV);" ~
        "vec3 hitObjectGetWorldRayOriginNV(hitObjectNV);" ~
        "vec3 hitObjectGetWorldRayDirectionNV(hitObjectNV);" ~
        "vec3 hitObjectGetObjectRayOriginNV(hitObjectNV);" ~
        "vec3 hitObjectGetObjectRayDirectionNV(hitObjectNV);" ~
        "mat4x3 hitObjectGetWorldToObjectNV(hitObjectNV);" ~
        "mat4x3 hitObjectGetObjectToWorldNV(hitObjectNV);" ~
        "int hitObjectGetInstanceCustomIndexNV(hitObjectNV);" ~
        "int hitObjectGetInstanceIdNV(hitObjectNV);" ~
        "int hitObjectGetGeometryIndexNV(hitObjectNV);" ~
        "int hitObjectGetPrimitiveIndexNV(hitObjectNV);" ~
        "uint hitObjectGetHitKindNV(hitObjectNV);" ~
        "void hitObjectGetAttributesNV(hitObjectNV,int);" ~
        "float hitObjectGetCurrentTimeNV(hitObjectNV);" ~
        "uint hitObjectGetShaderBindingTableRecordIndexNV(hitObjectNV);" ~
        "uvec2 hitObjectGetShaderRecordBufferHandleNV(hitObjectNV);" ~
        "int hitObjectGetClusterIdNV(hitObjectNV);" ~
        "void reorderThreadNV(uint, uint);" ~
        "void reorderThreadNV(hitObjectNV);" ~
        "void reorderThreadNV(hitObjectNV, uint, uint);" ~
        "vec3 fetchMicroTriangleVertexPositionNV(accelerationStructureEXT, int, int, int, ivec2);" ~
        "vec2 fetchMicroTriangleVertexBarycentricNV(accelerationStructureEXT, int, int, int, ivec2);" ~
        "vec3 hitObjectGetSpherePositionNV(hitObjectNV);" ~
        "float hitObjectGetSphereRadiusNV(hitObjectNV);" ~
        "void hitObjectGetLSSPositionsNV(hitObjectNV, out vec3[2]);" ~
        "void hitObjectGetLSSRadiiNV(hitObjectNV, out float[2]);" ~
        "bool hitObjectIsSphereHitNV(hitObjectNV);" ~
        "bool hitObjectIsLSSHitNV(hitObjectNV);" ~
        "void hitObjectTraceRayEXT(hitObjectEXT,accelerationStructureEXT,uint,uint,uint,uint,uint,vec3,float,vec3,float,int);" ~
        "void hitObjectTraceRayMotionEXT(hitObjectEXT,accelerationStructureEXT,uint,uint,uint,uint,uint,vec3,float,vec3,float,float,int);" ~
        "void hitObjectRecordMissEXT(hitObjectEXT,uint,uint,vec3,float,vec3,float);" ~
        "void hitObjectRecordMissMotionEXT(hitObjectEXT,uint,uint,vec3,float,vec3,float,float);" ~
        "void hitObjectRecordEmptyEXT(hitObjectEXT);" ~
        "void hitObjectExecuteShaderEXT(hitObjectEXT,int);" ~
        "bool hitObjectIsEmptyEXT(hitObjectEXT);" ~
        "bool hitObjectIsMissEXT(hitObjectEXT);" ~
        "bool hitObjectIsHitEXT(hitObjectEXT);" ~
        "float hitObjectGetRayTMinEXT(hitObjectEXT);" ~
        "float hitObjectGetRayTMaxEXT(hitObjectEXT);" ~
        "uint hitObjectGetRayFlagsEXT(hitObjectEXT);" ~
        "vec3 hitObjectGetWorldRayOriginEXT(hitObjectEXT);" ~
        "vec3 hitObjectGetWorldRayDirectionEXT(hitObjectEXT);" ~
        "vec3 hitObjectGetObjectRayOriginEXT(hitObjectEXT);" ~
        "vec3 hitObjectGetObjectRayDirectionEXT(hitObjectEXT);" ~
        "mat4x3 hitObjectGetWorldToObjectEXT(hitObjectEXT);" ~
        "mat4x3 hitObjectGetObjectToWorldEXT(hitObjectEXT);" ~
        "int hitObjectGetInstanceCustomIndexEXT(hitObjectEXT);" ~
        "int hitObjectGetInstanceIdEXT(hitObjectEXT);" ~
        "int hitObjectGetGeometryIndexEXT(hitObjectEXT);" ~
        "int hitObjectGetPrimitiveIndexEXT(hitObjectEXT);" ~
        "uint hitObjectGetHitKindEXT(hitObjectEXT);" ~
        "void hitObjectGetAttributesEXT(hitObjectEXT,int);" ~
        "float hitObjectGetCurrentTimeEXT(hitObjectEXT);" ~
        "uint hitObjectGetShaderBindingTableRecordIndexEXT(hitObjectEXT);" ~
        "uvec2 hitObjectGetShaderRecordBufferHandleEXT(hitObjectEXT);" ~
        "void hitObjectSetShaderBindingTableRecordIndexEXT(hitObjectEXT, uint);" ~
        "void hitObjectReorderExecuteEXT(hitObjectEXT,int);" ~
        "void hitObjectReorderExecuteEXT(hitObjectEXT,uint,uint,int);" ~
        "void hitObjectTraceReorderExecuteEXT(hitObjectEXT,accelerationStructureEXT,uint,uint,uint,uint,uint,vec3,float,vec3,float,int);" ~
        "void hitObjectTraceReorderExecuteEXT(hitObjectEXT,accelerationStructureEXT,uint,uint,uint,uint,uint,vec3,float,vec3,float,uint,uint,int);" ~
        "void hitObjectTraceMotionReorderExecuteEXT(hitObjectEXT,accelerationStructureEXT,uint,uint,uint,uint,uint,vec3,float,vec3,float,float,int);" ~
        "void hitObjectTraceMotionReorderExecuteEXT(hitObjectEXT,accelerationStructureEXT,uint,uint,uint,uint,uint,vec3,float,vec3,float,float,uint,uint,int);" ~
        "void hitObjectRecordFromQueryEXT(hitObjectEXT, rayQueryEXT, uint, int);" ~
        "void hitObjectGetIntersectionTriangleVertexPositionsEXT(hitObjectEXT, out vec3[3]);" ~
        "void reorderThreadEXT(uint, uint);" ~
        "void reorderThreadEXT(hitObjectEXT);" ~
        "void reorderThreadEXT(hitObjectEXT, uint, uint);" ~
        "\n"
      );
      stageBuiltins.STAGE_INTERSECT ~= (
        "bool reportIntersectionNV(float, uint);" ~
        "bool reportIntersectionEXT(float, uint);" ~
        "\n"
      );
      stageBuiltins.STAGE_ANYHIT ~= (
        "void ignoreIntersectionNV();" ~
        "void terminateRayNV();" ~
        "\n"
      );
      stageBuiltins.STAGE_CLOSESTHIT ~= (
        "void traceNV(accelerationStructureNV,uint,uint,uint,uint,uint,vec3,float,vec3,float,int);" ~
        "void traceRayMotionNV(accelerationStructureNV,uint,uint,uint,uint,uint,vec3,float,vec3,float,float,int);" ~
        "void traceRayEXT(accelerationStructureEXT,uint,uint,uint,uint,uint,vec3,float,vec3,float,int);" ~
        "void executeCallableNV(uint, int);" ~
        "void executeCallableEXT(uint, int);" ~
        "void hitObjectTraceRayNV(hitObjectNV,accelerationStructureEXT,uint,uint,uint,uint,uint,vec3,float,vec3,float,int);" ~
        "void hitObjectTraceRayMotionNV(hitObjectNV,accelerationStructureEXT,uint,uint,uint,uint,uint,vec3,float,vec3,float,float,int);" ~
        "void hitObjectRecordHitNV(hitObjectNV,accelerationStructureEXT,int,int,int,uint,uint,uint,vec3,float,vec3,float,int);" ~
        "void hitObjectRecordHitMotionNV(hitObjectNV,accelerationStructureEXT,int,int,int,uint,uint,uint,vec3,float,vec3,float,float,int);" ~
        "void hitObjectRecordHitWithIndexNV(hitObjectNV,accelerationStructureEXT,int,int,int,uint,uint,vec3,float,vec3,float,int);" ~
        "void hitObjectRecordHitWithIndexMotionNV(hitObjectNV, accelerationStructureEXT,int,int,int,uint,uint,vec3,float,vec3,float,float,int);" ~
        "void hitObjectRecordMissNV(hitObjectNV, uint, vec3, float, vec3, float);" ~
        "void hitObjectRecordMissMotionNV(hitObjectNV,uint,vec3,float,vec3,float,float);" ~
        "void hitObjectRecordEmptyNV(hitObjectNV);" ~
        "void hitObjectExecuteShaderNV(hitObjectNV, int);" ~
        "bool hitObjectIsEmptyNV(hitObjectNV);" ~
        "bool hitObjectIsMissNV(hitObjectNV);" ~
        "bool hitObjectIsHitNV(hitObjectNV);" ~
        "float hitObjectGetRayTMinNV(hitObjectNV);" ~
        "float hitObjectGetRayTMaxNV(hitObjectNV);" ~
        "vec3 hitObjectGetWorldRayOriginNV(hitObjectNV);" ~
        "vec3 hitObjectGetWorldRayDirectionNV(hitObjectNV);" ~
        "vec3 hitObjectGetObjectRayOriginNV(hitObjectNV);" ~
        "vec3 hitObjectGetObjectRayDirectionNV(hitObjectNV);" ~
        "mat4x3 hitObjectGetWorldToObjectNV(hitObjectNV);" ~
        "mat4x3 hitObjectGetObjectToWorldNV(hitObjectNV);" ~
        "int hitObjectGetInstanceCustomIndexNV(hitObjectNV);" ~
        "int hitObjectGetInstanceIdNV(hitObjectNV);" ~
        "int hitObjectGetGeometryIndexNV(hitObjectNV);" ~
        "int hitObjectGetPrimitiveIndexNV(hitObjectNV);" ~
        "uint hitObjectGetHitKindNV(hitObjectNV);" ~
        "void hitObjectGetAttributesNV(hitObjectNV,int);" ~
        "float hitObjectGetCurrentTimeNV(hitObjectNV);" ~
        "uint hitObjectGetShaderBindingTableRecordIndexNV(hitObjectNV);" ~
        "uvec2 hitObjectGetShaderRecordBufferHandleNV(hitObjectNV);" ~
        "int hitObjectGetClusterIdNV(hitObjectNV);" ~
        "vec3 hitObjectGetSpherePositionNV(hitObjectNV);" ~
        "float hitObjectGetSphereRadiusNV(hitObjectNV);" ~
        "void hitObjectGetLSSPositionsNV(hitObjectNV, out vec3[2]);" ~
        "void hitObjectGetLSSRadiiNV(hitObjectNV, out float[2]);" ~
        "bool hitObjectIsSphereHitNV(hitObjectNV);" ~
        "bool hitObjectIsLSSHitNV(hitObjectNV);" ~
        "void hitObjectTraceRayEXT(hitObjectEXT,accelerationStructureEXT,uint,uint,uint,uint,uint,vec3,float,vec3,float,int);" ~
        "void hitObjectTraceRayMotionEXT(hitObjectEXT,accelerationStructureEXT,uint,uint,uint,uint,uint,vec3,float,vec3,float,float,int);" ~
        "void hitObjectRecordMissEXT(hitObjectEXT,uint,uint,vec3,float,vec3,float);" ~
        "void hitObjectRecordMissMotionEXT(hitObjectEXT,uint,uint,vec3,float,vec3,float,float);" ~
        "void hitObjectRecordEmptyEXT(hitObjectEXT);" ~
        "void hitObjectExecuteShaderEXT(hitObjectEXT,int);" ~
        "bool hitObjectIsEmptyEXT(hitObjectEXT);" ~
        "bool hitObjectIsMissEXT(hitObjectEXT);" ~
        "bool hitObjectIsHitEXT(hitObjectEXT);" ~
        "float hitObjectGetRayTMinEXT(hitObjectEXT);" ~
        "float hitObjectGetRayTMaxEXT(hitObjectEXT);" ~
        "uint hitObjectGetRayFlagsEXT(hitObjectEXT);" ~
        "vec3 hitObjectGetWorldRayOriginEXT(hitObjectEXT);" ~
        "vec3 hitObjectGetWorldRayDirectionEXT(hitObjectEXT);" ~
        "vec3 hitObjectGetObjectRayOriginEXT(hitObjectEXT);" ~
        "vec3 hitObjectGetObjectRayDirectionEXT(hitObjectEXT);" ~
        "mat4x3 hitObjectGetWorldToObjectEXT(hitObjectEXT);" ~
        "mat4x3 hitObjectGetObjectToWorldEXT(hitObjectEXT);" ~
        "int hitObjectGetInstanceCustomIndexEXT(hitObjectEXT);" ~
        "int hitObjectGetInstanceIdEXT(hitObjectEXT);" ~
        "int hitObjectGetGeometryIndexEXT(hitObjectEXT);" ~
        "int hitObjectGetPrimitiveIndexEXT(hitObjectEXT);" ~
        "uint hitObjectGetHitKindEXT(hitObjectEXT);" ~
        "void hitObjectGetAttributesEXT(hitObjectEXT,int);" ~
        "float hitObjectGetCurrentTimeEXT(hitObjectEXT);" ~
        "uint hitObjectGetShaderBindingTableRecordIndexEXT(hitObjectEXT);" ~
        "uvec2 hitObjectGetShaderRecordBufferHandleEXT(hitObjectEXT);" ~
        "void hitObjectSetShaderBindingTableRecordIndexEXT(hitObjectEXT, uint);" ~
        "void hitObjectRecordFromQueryEXT(hitObjectEXT, rayQueryEXT,uint, int);" ~
        "void hitObjectGetIntersectionTriangleVertexPositionsEXT(hitObjectEXT, out vec3[3]);" ~
        "\n"
      );
      stageBuiltins.STAGE_MISS ~= (
        "void traceNV(accelerationStructureNV,uint,uint,uint,uint,uint,vec3,float,vec3,float,int);" ~
        "void traceRayMotionNV(accelerationStructureNV,uint,uint,uint,uint,uint,vec3,float,vec3,float,float,int);" ~
        "void traceRayEXT(accelerationStructureEXT,uint,uint,uint,uint,uint,vec3,float,vec3,float,int);" ~
        "void executeCallableNV(uint, int);" ~
        "void executeCallableEXT(uint, int);" ~
        "void hitObjectTraceRayNV(hitObjectNV,accelerationStructureEXT,uint,uint,uint,uint,uint,vec3,float,vec3,float,int);" ~
        "void hitObjectTraceRayMotionNV(hitObjectNV,accelerationStructureEXT,uint,uint,uint,uint,uint,vec3,float,vec3,float,float,int);" ~
        "void hitObjectRecordHitNV(hitObjectNV,accelerationStructureEXT,int,int,int,uint,uint,uint,vec3,float,vec3,float,int);" ~
        "void hitObjectRecordHitMotionNV(hitObjectNV,accelerationStructureEXT,int,int,int,uint,uint,uint,vec3,float,vec3,float,float,int);" ~
        "void hitObjectRecordHitWithIndexNV(hitObjectNV,accelerationStructureEXT,int,int,int,uint,uint,vec3,float,vec3,float,int);" ~
        "void hitObjectRecordHitWithIndexMotionNV(hitObjectNV, accelerationStructureEXT,int,int,int,uint,uint,vec3,float,vec3,float,float,int);" ~
        "void hitObjectRecordMissNV(hitObjectNV, uint, vec3, float, vec3, float);" ~
        "void hitObjectRecordMissMotionNV(hitObjectNV,uint,vec3,float,vec3,float,float);" ~
        "void hitObjectRecordEmptyNV(hitObjectNV);" ~
        "void hitObjectExecuteShaderNV(hitObjectNV, int);" ~
        "bool hitObjectIsEmptyNV(hitObjectNV);" ~
        "bool hitObjectIsMissNV(hitObjectNV);" ~
        "bool hitObjectIsHitNV(hitObjectNV);" ~
        "float hitObjectGetRayTMinNV(hitObjectNV);" ~
        "float hitObjectGetRayTMaxNV(hitObjectNV);" ~
        "vec3 hitObjectGetWorldRayOriginNV(hitObjectNV);" ~
        "vec3 hitObjectGetWorldRayDirectionNV(hitObjectNV);" ~
        "vec3 hitObjectGetObjectRayOriginNV(hitObjectNV);" ~
        "vec3 hitObjectGetObjectRayDirectionNV(hitObjectNV);" ~
        "mat4x3 hitObjectGetWorldToObjectNV(hitObjectNV);" ~
        "mat4x3 hitObjectGetObjectToWorldNV(hitObjectNV);" ~
        "int hitObjectGetInstanceCustomIndexNV(hitObjectNV);" ~
        "int hitObjectGetInstanceIdNV(hitObjectNV);" ~
        "int hitObjectGetGeometryIndexNV(hitObjectNV);" ~
        "int hitObjectGetPrimitiveIndexNV(hitObjectNV);" ~
        "uint hitObjectGetHitKindNV(hitObjectNV);" ~
        "void hitObjectGetAttributesNV(hitObjectNV,int);" ~
        "float hitObjectGetCurrentTimeNV(hitObjectNV);" ~
        "uint hitObjectGetShaderBindingTableRecordIndexNV(hitObjectNV);" ~
        "uvec2 hitObjectGetShaderRecordBufferHandleNV(hitObjectNV);" ~
        "int hitObjectGetClusterIdNV(hitObjectNV);" ~
        "vec3 hitObjectGetSpherePositionNV(hitObjectNV);" ~
        "float hitObjectGetSphereRadiusNV(hitObjectNV);" ~
        "void hitObjectGetLSSPositionsNV(hitObjectNV, out vec3[2]);" ~
        "void hitObjectGetLSSRadiiNV(hitObjectNV, out float[2]);" ~
        "bool hitObjectIsSphereHitNV(hitObjectNV);" ~
        "bool hitObjectIsLSSHitNV(hitObjectNV);" ~
        "void hitObjectTraceRayEXT(hitObjectEXT,accelerationStructureEXT,uint,uint,uint,uint,uint,vec3,float,vec3,float,int);" ~
        "void hitObjectTraceRayMotionEXT(hitObjectEXT,accelerationStructureEXT,uint,uint,uint,uint,uint,vec3,float,vec3,float,float,int);" ~
        "void hitObjectRecordMissEXT(hitObjectEXT,uint,uint,vec3,float,vec3,float);" ~
        "void hitObjectRecordMissMotionEXT(hitObjectEXT,uint,uint,vec3,float,vec3,float,float);" ~
        "void hitObjectRecordEmptyEXT(hitObjectEXT);" ~
        "void hitObjectExecuteShaderEXT(hitObjectEXT,int);" ~
        "bool hitObjectIsEmptyEXT(hitObjectEXT);" ~
        "bool hitObjectIsMissEXT(hitObjectEXT);" ~
        "bool hitObjectIsHitEXT(hitObjectEXT);" ~
        "float hitObjectGetRayTMinEXT(hitObjectEXT);" ~
        "float hitObjectGetRayTMaxEXT(hitObjectEXT);" ~
        "uint hitObjectGetRayFlagsEXT(hitObjectEXT);" ~
        "vec3 hitObjectGetWorldRayOriginEXT(hitObjectEXT);" ~
        "vec3 hitObjectGetWorldRayDirectionEXT(hitObjectEXT);" ~
        "vec3 hitObjectGetObjectRayOriginEXT(hitObjectEXT);" ~
        "vec3 hitObjectGetObjectRayDirectionEXT(hitObjectEXT);" ~
        "mat4x3 hitObjectGetWorldToObjectEXT(hitObjectEXT);" ~
        "mat4x3 hitObjectGetObjectToWorldEXT(hitObjectEXT);" ~
        "int hitObjectGetInstanceCustomIndexEXT(hitObjectEXT);" ~
        "int hitObjectGetInstanceIdEXT(hitObjectEXT);" ~
        "int hitObjectGetGeometryIndexEXT(hitObjectEXT);" ~
        "int hitObjectGetPrimitiveIndexEXT(hitObjectEXT);" ~
        "uint hitObjectGetHitKindEXT(hitObjectEXT);" ~
        "void hitObjectGetAttributesEXT(hitObjectEXT,int);" ~
        "float hitObjectGetCurrentTimeEXT(hitObjectEXT);" ~
        "uint hitObjectGetShaderBindingTableRecordIndexEXT(hitObjectEXT);" ~
        "uvec2 hitObjectGetShaderRecordBufferHandleEXT(hitObjectEXT);" ~
        "void hitObjectSetShaderBindingTableRecordIndexEXT(hitObjectEXT, uint);" ~
        "void hitObjectRecordFromQueryEXT(hitObjectEXT, rayQueryEXT, uint, int);" ~
        "void hitObjectGetIntersectionTriangleVertexPositionsEXT(hitObjectEXT, out vec3[3]);" ~
        "\n"
      );
      stageBuiltins.STAGE_CALLABLE ~= (
        "void executeCallableNV(uint, int);" ~
        "void executeCallableEXT(uint, int);" ~
        "\n"
      );
    }

    if ((profile == EProfile(ES_PROFILE: 1) && version_ >= 320) ||
      (profile != EProfile(ES_PROFILE: 1) && version_ >= 450)) {
      stageBuiltins.STAGE_COMPUTE ~= derivativeControls;
      stageBuiltins.STAGE_COMPUTE ~= "\n";
    }
    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 450) {
      stageBuiltins.STAGE_COMPUTE ~= derivativesAndControl16bits;
      stageBuiltins.STAGE_COMPUTE ~= derivativesAndControl64bits;
      stageBuiltins.STAGE_COMPUTE ~= "\n";
    }

    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 450) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 320)) {
      stageBuiltins.STAGE_MESH ~= (
        "void writePackedPrimitiveIndices4x8NV(uint, uint);" ~
        "\n"
      );
    }
    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 450) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 320)) {
      stageBuiltins.STAGE_TASK ~= (
        "void EmitMeshTasksEXT(uint, uint, uint);" ~
        "\n"
      );
      stageBuiltins.STAGE_MESH ~= (
        "void SetMeshOutputsEXT(uint, uint);" ~
        "\n"
      );
    }
    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 460) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 320)) {
      stageBuiltins.STAGE_MESH ~= (
        "vec3 fetchMicroTriangleVertexPositionNV(accelerationStructureEXT, int, int, int, ivec2);" ~
        "vec2 fetchMicroTriangleVertexBarycentricNV(accelerationStructureEXT, int, int, int, ivec2);" ~
        "\n"
      );
      stageBuiltins.STAGE_COMPUTE ~= (
        "vec3 fetchMicroTriangleVertexPositionNV(accelerationStructureEXT, int, int, int, ivec2);" ~
        "vec2 fetchMicroTriangleVertexBarycentricNV(accelerationStructureEXT, int, int, int, ivec2);" ~
        "\n"
      );
    }

    if (spvVersion.spv == 0) {
      commonBuiltins ~= "struct gl_DepthRangeParameters {";
      if (profile == EProfile(ES_PROFILE: 1)) {
        commonBuiltins ~= (
          "highp float near;" ~
          "highp float far;" ~
          "highp float diff;"
        );
      } else {
        commonBuiltins ~= (
          "float near;" ~
          "float far;" ~
          "float diff;"
        );
      }
      commonBuiltins ~= "};";
      commonBuiltins ~= "uniform gl_DepthRangeParameters gl_DepthRange;";
      commonBuiltins ~= "\n";
    }

    if (spvVersion.spv == 0 && IncludeLegacy(version_, profile, spvVersion)) {
      commonBuiltins ~= (
        "uniform mat4 gl_ModelViewMatrix;" ~
        "uniform mat4 gl_ProjectionMatrix;" ~
        "uniform mat4 gl_ModelViewProjectionMatrix;" ~

        "uniform mat3 gl_NormalMatrix;" ~

        "uniform mat4 gl_ModelViewMatrixInverse;" ~
        "uniform mat4 gl_ProjectionMatrixInverse;" ~
        "uniform mat4 gl_ModelViewProjectionMatrixInverse;" ~

        "uniform mat4 gl_ModelViewMatrixTranspose;" ~
        "uniform mat4 gl_ProjectionMatrixTranspose;" ~
        "uniform mat4 gl_ModelViewProjectionMatrixTranspose;" ~

        "uniform mat4 gl_ModelViewMatrixInverseTranspose;" ~
        "uniform mat4 gl_ProjectionMatrixInverseTranspose;" ~
        "uniform mat4 gl_ModelViewProjectionMatrixInverseTranspose;" ~

        "uniform float gl_NormalScale;" ~

        "struct gl_PointParameters {" ~
          "float size;" ~
          "float sizeMin;" ~
          "float sizeMax;" ~
          "float fadeThresholdSize;" ~
          "float distanceConstantAttenuation;" ~
          "float distanceLinearAttenuation;" ~
          "float distanceQuadraticAttenuation;" ~
        "};" ~

        "uniform gl_PointParameters gl_Point;" ~

        "struct gl_MaterialParameters {" ~
          "vec4 emission;" ~
          "vec4 ambient;" ~
          "vec4 diffuse;" ~
          "vec4 specular;" ~
          "float shininess;" ~
        "};" ~
        "uniform gl_MaterialParameters gl_FrontMaterial;" ~
        "uniform gl_MaterialParameters gl_BackMaterial;" ~

        "struct gl_LightSourceParameters {" ~
          "vec4 ambient;" ~
          "vec4 diffuse;" ~
          "vec4 specular;" ~
          "vec4 position;" ~
          "vec4 halfVector;" ~
          "vec3 spotDirection;" ~
          "float spotExponent;" ~
          "float spotCutoff;" ~

          "float spotCosCutoff;" ~

          "float constantAttenuation;" ~
          "float linearAttenuation;" ~
          "float quadraticAttenuation;" ~
        "};" ~

        "struct gl_LightModelParameters {" ~
          "vec4 ambient;" ~
        "};" ~

        "uniform gl_LightModelParameters gl_LightModel;" ~

        "struct gl_LightModelProducts {" ~
          "vec4 sceneColor;" ~
        "};" ~

        "uniform gl_LightModelProducts gl_FrontLightModelProduct;" ~
        "uniform gl_LightModelProducts gl_BackLightModelProduct;" ~

        "struct gl_LightProducts {" ~
          "vec4 ambient;" ~
          "vec4 diffuse;" ~
          "vec4 specular;" ~
        "};" ~

        "struct gl_FogParameters {" ~
          "vec4 color;" ~
          "float density;" ~
          "float start;" ~
          "float end;" ~
          "float scale;" ~
        "};" ~

        "uniform gl_FogParameters gl_Fog;" ~

        "\n"
      );
    }

    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 420) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 310)) {
      stageBuiltins.STAGE_COMPUTE ~= (
        "in highp uvec3 gl_NumWorkGroups;" ~
        "const highp uvec3 gl_WorkGroupSize = uvec3(1,1,1);" ~

        "in highp uvec3 gl_WorkGroupID;" ~
        "in highp uvec3 gl_LocalInvocationID;" ~

        "in highp uvec3 gl_GlobalInvocationID;" ~
        "in highp uint gl_LocalInvocationIndex;" ~

        "\n"
      );
    }

    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 140) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 310)) {
      stageBuiltins.STAGE_COMPUTE ~= (
        "in highp int gl_DeviceIndex;" ~
        "\n"
      );
    }

    if ((profile == EProfile(ES_PROFILE: 1) && version_ >= 310) ||
      (profile != EProfile(ES_PROFILE: 1) && version_ >= 460)) {
      stageBuiltins.STAGE_COMPUTE ~= (
        "in highp uvec2 gl_TileOffsetQCOM;" ~
        "in highp uvec3 gl_TileDimensionQCOM;" ~
        "in highp uvec2 gl_TileApronSizeQCOM;" ~
        "\n"
      );
    }

    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 450) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 320)) {
      stageBuiltins.STAGE_MESH ~= (
        "out gl_MeshPerVertexNV {" ~
          "vec4 gl_Position;" ~
          "float gl_PointSize;" ~
          "float gl_ClipDistance[];" ~
          "float gl_CullDistance[];" ~
          "perviewNV vec4 gl_PositionPerViewNV[];" ~
          "perviewNV float gl_ClipDistancePerViewNV[][];" ~
          "perviewNV float gl_CullDistancePerViewNV[][];" ~
        "} gl_MeshVerticesNV[];" ~

        "perprimitiveNV out gl_MeshPerPrimitiveNV {" ~
          "int gl_PrimitiveID;" ~
          "int gl_Layer;" ~
          "int gl_ViewportIndex;" ~
          "int gl_ViewportMask[];" ~
          "perviewNV int gl_LayerPerViewNV[];" ~
          "perviewNV int gl_ViewportMaskPerViewNV[][];" ~
        "} gl_MeshPrimitivesNV[];" ~

        "out uint gl_PrimitiveCountNV;" ~
        "out uint gl_PrimitiveIndicesNV[];" ~

        "in uint gl_MeshViewCountNV;" ~
        "in uint gl_MeshViewIndicesNV[4];" ~

        "const highp uvec3 gl_WorkGroupSize = uvec3(1,1,1);" ~

        "in highp uvec3 gl_WorkGroupID;" ~
        "in highp uvec3 gl_LocalInvocationID;" ~

        "in highp uvec3 gl_GlobalInvocationID;" ~
        "in highp uint gl_LocalInvocationIndex;" ~
        "\n" ~

        "out uint gl_PrimitivePointIndicesEXT[];" ~
        "out uvec2 gl_PrimitiveLineIndicesEXT[];" ~
        "out uvec3 gl_PrimitiveTriangleIndicesEXT[];" ~
        "in highp uvec3 gl_NumWorkGroups;" ~
        "\n" ~

        "out gl_MeshPerVertexEXT {" ~
          "vec4 gl_Position;" ~
          "float gl_PointSize;" ~
          "float gl_ClipDistance[];" ~
          "float gl_CullDistance[];" ~
        "} gl_MeshVerticesEXT[];" ~

        "perprimitiveEXT out gl_MeshPerPrimitiveEXT {" ~
          "int gl_PrimitiveID;" ~
          "int gl_Layer;" ~
          "int gl_ViewportIndex;" ~
          "bool gl_CullPrimitiveEXT;" ~
          "int gl_PrimitiveShadingRateEXT;" ~
        "} gl_MeshPrimitivesEXT[];"
      );

      stageBuiltins.STAGE_TASK ~= (
        "out uint gl_TaskCountNV;" ~

        "const highp uvec3 gl_WorkGroupSize = uvec3(1,1,1);" ~

        "in highp uvec3 gl_WorkGroupID;" ~
        "in highp uvec3 gl_LocalInvocationID;" ~

        "in highp uvec3 gl_GlobalInvocationID;" ~
        "in highp uint gl_LocalInvocationIndex;" ~

        "in uint gl_MeshViewCountNV;" ~
        "in uint gl_MeshViewIndicesNV[4];" ~
        "in highp uvec3 gl_NumWorkGroups;" ~
        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 450) {
      stageBuiltins.STAGE_MESH ~= (
        "in highp int gl_DeviceIndex;" ~
        "in int gl_DrawIDARB;" ~
        "in int gl_ViewIndex;" ~
        "\n"
      );

      stageBuiltins.STAGE_TASK ~= (
        "in highp int gl_DeviceIndex;" ~
        "in int gl_DrawIDARB;" ~
        "\n"
      );

      if (version_ >= 460) {
        stageBuiltins.STAGE_MESH ~= (
          "in int gl_DrawID;" ~
          "\n"
        );
        stageBuiltins.STAGE_TASK ~= (
          "in int gl_DrawID;" ~
          "\n"
        );
      }
    }

    if (profile != EProfile(ES_PROFILE: 1)) {
      if (version_ < 130) {
        stageBuiltins.STAGE_VERTEX ~= (
          "attribute vec4 gl_Color;" ~
          "attribute vec4 gl_SecondaryColor;" ~
          "attribute vec3 gl_Normal;" ~
          "attribute vec4 gl_Vertex;" ~
          "attribute vec4 gl_MultiTexCoord0;" ~
          "attribute vec4 gl_MultiTexCoord1;" ~
          "attribute vec4 gl_MultiTexCoord2;" ~
          "attribute vec4 gl_MultiTexCoord3;" ~
          "attribute vec4 gl_MultiTexCoord4;" ~
          "attribute vec4 gl_MultiTexCoord5;" ~
          "attribute vec4 gl_MultiTexCoord6;" ~
          "attribute vec4 gl_MultiTexCoord7;" ~
          "attribute float gl_FogCoord;" ~
          "\n"
        );
      } else if (IncludeLegacy(version_, profile, spvVersion)) {
        stageBuiltins.STAGE_VERTEX ~= (
          "in vec4 gl_Color;" ~
          "in vec4 gl_SecondaryColor;" ~
          "in vec3 gl_Normal;" ~
          "in vec4 gl_Vertex;" ~
          "in vec4 gl_MultiTexCoord0;" ~
          "in vec4 gl_MultiTexCoord1;" ~
          "in vec4 gl_MultiTexCoord2;" ~
          "in vec4 gl_MultiTexCoord3;" ~
          "in vec4 gl_MultiTexCoord4;" ~
          "in vec4 gl_MultiTexCoord5;" ~
          "in vec4 gl_MultiTexCoord6;" ~
          "in vec4 gl_MultiTexCoord7;" ~
          "in float gl_FogCoord;" ~
          "\n"
        );
      }

      if (version_ < 150) {
        if (version_ < 130) {
          stageBuiltins.STAGE_VERTEX ~= (
            "vec4 gl_ClipVertex;" ~
            "varying vec4 gl_FrontColor;" ~
            "varying vec4 gl_BackColor;" ~
            "varying vec4 gl_FrontSecondaryColor;" ~
            "varying vec4 gl_BackSecondaryColor;" ~
            "varying vec4 gl_TexCoord[];" ~
            "varying float gl_FogFragCoord;" ~
            "\n"
          );
        } else if (IncludeLegacy(version_, profile, spvVersion)) {
          stageBuiltins.STAGE_VERTEX ~= (
            "vec4 gl_ClipVertex;" ~
            "out vec4 gl_FrontColor;" ~
            "out vec4 gl_BackColor;" ~
            "out vec4 gl_FrontSecondaryColor;" ~
            "out vec4 gl_BackSecondaryColor;" ~
            "out vec4 gl_TexCoord[];" ~
            "out float gl_FogFragCoord;" ~
            "\n"
          );
        }
        stageBuiltins.STAGE_VERTEX ~= (
          "vec4 gl_Position;" ~
          "float gl_PointSize;"
        );

        if (version_ == 130 || version_ == 140)
          stageBuiltins.STAGE_VERTEX ~=
            "out float gl_ClipDistance[];";
      } else {
        stageBuiltins.STAGE_VERTEX ~= "out gl_PerVertex {" ~
          "vec4 gl_Position;" ~
          "float gl_PointSize;" ~
          "float gl_ClipDistance[];";

        if (IncludeLegacy(version_, profile, spvVersion))
          stageBuiltins.STAGE_VERTEX ~= (
            "vec4 gl_ClipVertex;" ~
            "vec4 gl_FrontColor;" ~
            "vec4 gl_BackColor;" ~
            "vec4 gl_FrontSecondaryColor;" ~
            "vec4 gl_BackSecondaryColor;" ~
            "vec4 gl_TexCoord[];" ~
            "float gl_FogFragCoord;"
          );
        if (version_ >= 450)
          stageBuiltins.STAGE_VERTEX ~= "float gl_CullDistance[];";
        stageBuiltins.STAGE_VERTEX ~= "};";
        stageBuiltins.STAGE_VERTEX ~= "\n";
      }
      if (version_ >= 130 && spvVersion.vulkan == 0)
        stageBuiltins.STAGE_VERTEX ~=
          "int gl_VertexID;";
      if (spvVersion.vulkan == 0)
        stageBuiltins.STAGE_VERTEX ~=
          "int gl_InstanceID;";
      if (spvVersion.vulkan > 0 && version_ >= 140)
        stageBuiltins.STAGE_VERTEX ~= (
          "in int gl_VertexIndex;" ~
          "in int gl_InstanceIndex;"
        );
      if (spvVersion.vulkan > 0 && version_ >= 140 && spvVersion.vulkanRelaxed)
        stageBuiltins.STAGE_VERTEX ~= (
          "in int gl_VertexID;" ~
          "in int gl_InstanceID;"
        );
      if (version_ >= 440) {
        stageBuiltins.STAGE_VERTEX ~= (
          "in int gl_BaseVertexARB;" ~
          "in int gl_BaseInstanceARB;" ~
          "in int gl_DrawIDARB;"
        );
      }
      if (version_ >= 410) {
        stageBuiltins.STAGE_VERTEX ~= (
          "out int gl_ViewportIndex;" ~
          "out int gl_Layer;"
        );
      }
      if (version_ >= 460) {
        stageBuiltins.STAGE_VERTEX ~= (
          "in int gl_BaseVertex;" ~
          "in int gl_BaseInstance;" ~
          "in int gl_DrawID;"
        );
      }
      if (version_ >= 430) {
        stageBuiltins.STAGE_VERTEX ~=
          "out int gl_ViewportMask[];";
      }
      if (version_ >= 450) {
        stageBuiltins.STAGE_VERTEX ~= (
          "out int gl_SecondaryViewportMaskNV[];" ~
          "out vec4 gl_SecondaryPositionNV;" ~
          "out vec4 gl_PositionPerViewNV[];" ~
          "out int gl_ViewportMaskPerViewNV[];"
        );
      }
    } else {
      if (version_ == 100) {
        stageBuiltins.STAGE_VERTEX ~= (
          "highp vec4 gl_Position;" ~
          "mediump float gl_PointSize;" ~
          "highp int gl_InstanceID;"
        );
      } else {
        if (spvVersion.vulkan == 0 || spvVersion.vulkanRelaxed)
          stageBuiltins.STAGE_VERTEX ~= (
            "in highp int gl_VertexID;" ~
            "in highp int gl_InstanceID;"
          );
        if (spvVersion.vulkan > 0)
          stageBuiltins.STAGE_VERTEX ~= (
            "in highp int gl_VertexIndex;" ~
            "in highp int gl_InstanceIndex;"
          );
        if (version_ < 310)
          stageBuiltins.STAGE_VERTEX ~= (
            "highp vec4 gl_Position;" ~
            "highp float gl_PointSize;"
          );
        else
          stageBuiltins.STAGE_VERTEX ~= (
            "out gl_PerVertex {" ~
              "highp vec4 gl_Position;" ~
              "highp float gl_PointSize;" ~
            "};"
          );
      }
    }

    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 140) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 310)) {
      stageBuiltins.STAGE_VERTEX ~= (
        "in highp int gl_DeviceIndex;" ~
        "in highp int gl_ViewIndex;"
      );
    }

    if (version_ >= 300) {
      stageBuiltins.STAGE_VERTEX ~= (
        "in highp uint gl_ViewID_OVR;" ~
        "\n"
      );
    }

    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 450) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 310)) {
      stageBuiltins.STAGE_VERTEX ~= (
        "out highp int gl_PrimitiveShadingRateEXT;" ~
        "\n"
      );
    }

    if (profile == EProfile(CORE_PROFILE: 1) ||
      profile == EProfile(COMPATIBILITY_PROFILE: 1)) {
      stageBuiltins.STAGE_GEOMETRY ~= "in gl_PerVertex {";
      stageBuiltins.STAGE_GEOMETRY ~= (
        "vec4 gl_Position" ~
        "float gl_PointSize" ~
        "float gl_ClipDistance[]"
      );
      if (profile == EProfile(COMPATIBILITY_PROFILE: 1))
        stageBuiltins.STAGE_GEOMETRY ~= (
          "vec4 gl_ClipVertex;" ~
          "vec4 gl_FrontColor;" ~
          "vec4 gl_BackColor;" ~
          "vec4 gl_FrontSecondaryColor;" ~
          "vec4 gl_BackSecondaryColor;" ~
          "vec4 gl_TexCoord[];" ~
          "float gl_FogFragCoord;"
        );
      if (version_ >= 450)
        stageBuiltins.STAGE_GEOMETRY ~= (
          "float gl_CullDistance[];" ~
          "vec4 gl_SecondaryPositionNV;" ~
          "vec4 gl_PositionPerViewNV[];"
        );
      stageBuiltins.STAGE_GEOMETRY ~= "} gl_in[];";
      stageBuiltins.STAGE_GEOMETRY ~= "in int gl_PrimitiveIDIn;";
      stageBuiltins.STAGE_GEOMETRY ~= "out gl_PerVertex {";
      stageBuiltins.STAGE_GEOMETRY ~= (
        "vec4 gl_Position;" ~
        "float gl_PointSize;" ~
        "float gl_ClipDistance[];" ~
        "\n"
      );
      if (profile == EProfile(COMPATIBILITY_PROFILE: 1) && version_ >= 400)
        stageBuiltins.STAGE_GEOMETRY ~= (
          "vec4 gl_ClipVertex;" ~
          "vec4 gl_FrontColor;" ~
          "vec4 gl_BackColor;" ~
          "vec4 gl_FrontSecondaryColor;" ~
          "vec4 gl_BackSecondaryColor;" ~
          "vec4 gl_TexCoord[];" ~
          "float gl_FogFragCoord;"
        );
      if (version_ >= 450)
        stageBuiltins.STAGE_GEOMETRY ~= 
          "float gl_CullDistance[];";
      stageBuiltins.STAGE_GEOMETRY ~= "};";
      stageBuiltins.STAGE_GEOMETRY ~= (
        "out int gl_PrimitiveID;" ~
        "out int gl_Layer;"
      );

      if (version_ >= 150)
        stageBuiltins.STAGE_GEOMETRY ~=
          "out int gl_ViewportIndex;";

      if (profile == EProfile(COMPATIBILITY_PROFILE: 1) && version_ < 400)
        stageBuiltins.STAGE_GEOMETRY ~=
          "out vec4 gl_ClipVertex;";

      if (version_ >= 150)
        stageBuiltins.STAGE_GEOMETRY ~=
          "in int gl_InvocationID;";

      if (version_ >= 430)
        stageBuiltins.STAGE_GEOMETRY ~=
          "out int gl_ViewportMask[];";

      if (version_ >= 450)
        stageBuiltins.STAGE_GEOMETRY ~= (
          "out int gl_SecondaryViewportMaskNV[];" ~
          "out vec4 gl_SecondaryPositionNV;" ~
          "out vec4 gl_PositionPerViewNV[];" ~
          "out int gl_ViewportMaskPerViewNV[];"
        );
    } else if (profile == EProfile(ES_PROFILE: 1) && version_ >= 310) {
      stageBuiltins.STAGE_GEOMETRY ~= (
        "in gl_PerVertex {" ~
          "highp vec4 gl_Position;" ~
          "highp float gl_PointSize;" ~
        "} gl_in[];" ~
        "\n" ~
        "in highp int gl_PrimitiveIDIn;" ~
        "in highp int gl_InvocationID;" ~
        "\n" ~
        "out gl_PerVertex {" ~
          "highp vec4 gl_Position;" ~
          "highp float gl_PointSize;" ~
        "};" ~
        "\n" ~
        "out highp int gl_PrimitiveID;" ~
        "out highp int gl_Layer;" ~
        "\n"
      );
    }

    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 140) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 310)) {
      stageBuiltins.STAGE_GEOMETRY ~= (
        "in highp int gl_DeviceIndex;" ~
        "in highp int gl_ViewIndex;" ~
        "\n"
      );
    }

    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 450) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 310)) {
      stageBuiltins.STAGE_GEOMETRY ~= (
        "out highp int gl_PrimitiveShadingRateEXT;" ~
        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 150) {
      stageBuiltins.STAGE_TESSCONTROL ~= (
        "in int gl_PatchVerticesIn;" ~
        "in int gl_PrimitiveID;" ~
        "in int gl_InvocationID;"
      );
      stageBuiltins.STAGE_TESSCONTROL ~= "out gl_PerVertex {";
      stageBuiltins.STAGE_TESSCONTROL ~= (
        "vec4 gl_Position;" ~
        "float gl_PointSize;" ~
        "float gl_ClipDistance[];"
      );
      if (profile == EProfile(COMPATIBILITY_PROFILE: 1))
        stageBuiltins.STAGE_TESSCONTROL ~= (
          "vec4 gl_ClipVertex;" ~
          "vec4 gl_FrontColor;" ~
          "vec4 gl_BackColor;" ~
          "vec4 gl_FrontSecondaryColor;" ~
          "vec4 gl_BackSecondaryColor;" ~
          "vec4 gl_TexCoord[];" ~
          "float gl_FogFragCoord;"
        );
      if (version_ >= 450)
        stageBuiltins.STAGE_TESSCONTROL ~=
          "float gl_CullDistance[];";
      if (version_ >= 430)
        stageBuiltins.STAGE_TESSCONTROL ~=
          "int gl_ViewportMask[];";
      if (version_ >= 450)
        stageBuiltins.STAGE_TESSCONTROL ~= (
          "vec4 gl_SecondaryPositionNV;" ~
          "int gl_SecondaryViewportMaskNV[];" ~
          "vec4 gl_PositionPerViewNV[];" ~
          "int gl_ViewportMaskPerViewNV[];"
        );
      stageBuiltins.STAGE_TESSCONTROL ~= "} gl_out[];";
      stageBuiltins.STAGE_TESSCONTROL ~= (
        "patch out float gl_TessLevelOuter[4];" ~
        "patch out float gl_TessLevelInner[2];" ~
        "\n"
      );

      if (version_ >= 410)
        stageBuiltins.STAGE_TESSCONTROL ~= (
          "out int gl_ViewportIndex;" ~
          "out int gl_Layer;" ~
          "\n"
        );
    } else {
      stageBuiltins.STAGE_TESSCONTROL ~= (
        "in highp int gl_PatchVerticesIn;" ~
        "in highp int gl_PrimitiveID;" ~
        "in highp int gl_InvocationID;" ~
        
        "out gl_PerVertex {" ~
          "highp vec4 gl_Position;" ~
          "highp float gl_PointSize;" ~
        "} gl_out[];" ~

        "patch out highp float gl_TessLevelOuter[4];" ~
        "patch out highp float gl_TessLevelInner[2];" ~
        "patch out highp vec4 gl_BoundingBoxOES[2];" ~
        "patch out highp vec4 gl_BoundingBoxEXT[2];" ~
        "\n"
      );
      if (profile == EProfile(ES_PROFILE: 1) && version_ >= 320) {
        stageBuiltins.STAGE_TESSCONTROL ~= (
          "patch out highp vec4 gl_BoundingBox[2];" ~
          "\n"
        );
      }
    }

    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 140) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 310)) {
      stageBuiltins.STAGE_TESSCONTROL ~= (
        "in highp int gl_DeviceIndex;" ~
        "in highp int gl_ViewIndex;" ~
        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 150) {
      stageBuiltins.STAGE_TESSEVALUATION ~= (
        "in int gl_PatchVerticesIn;" ~
        "in int gl_PrimitiveID;" ~
        "in vec3 gl_TessCoord;" ~

        "patch in float gl_TessLevelOuter[4];" ~
        "patch in float gl_TessLevelInner[2];"
      );
      stageBuiltins.STAGE_TESSEVALUATION ~= "out gl_PerVertex {";
      stageBuiltins.STAGE_TESSEVALUATION ~= (
        "vec4 gl_Position;" ~
        "float gl_PointSize;" ~
        "float gl_ClipDistance[];"
      );
      if (version_ >= 400 && profile == EProfile(COMPATIBILITY_PROFILE: 1))
        stageBuiltins.STAGE_TESSEVALUATION ~= (
          "vec4 gl_ClipVertex;" ~
          "vec4 gl_FrontColor;" ~
          "vec4 gl_BackColor;" ~
          "vec4 gl_FrontSecondaryColor;" ~
          "vec4 gl_BackSecondaryColor;" ~
          "vec4 gl_TexCoord[];" ~
          "float gl_FogFragCoord;"
        );
      if (version_ >= 450)
        stageBuiltins.STAGE_TESSEVALUATION ~=
          "float gl_CullDistance[];";
      stageBuiltins.STAGE_TESSEVALUATION ~= "};";
      stageBuiltins.STAGE_TESSEVALUATION ~= "\n";

      if (version_ >= 410)
        stageBuiltins.STAGE_TESSEVALUATION ~= (
          "out int gl_ViewportIndex;" ~
          "out int gl_Layer;" ~
          "\n"
        );

      if (version_ >= 430)
        stageBuiltins.STAGE_TESSEVALUATION ~=
          "out int gl_ViewportMask[];";

      if (version_ >= 450)
        stageBuiltins.STAGE_TESSEVALUATION ~= (
          "out vec4 gl_SecondaryPositionNV;" ~
          "out int gl_SecondaryViewportMaskNV[];" ~
          "out vec4 gl_PositionPerViewNV[];" ~
          "out int gl_ViewportMaskPerViewNV[];"
        );
    } else if (profile == EProfile(ES_PROFILE: 1) && version_ >= 310) {
      stageBuiltins.STAGE_TESSEVALUATION ~= (
        "in highp int gl_PatchVerticesIn;" ~
        "in highp int gl_PrimitiveID;" ~
        "in highp vec3 gl_TessCoord;" ~

        "patch in highp float gl_TessLevelOuter[4];" ~
        "patch in highp float gl_TessLevelInner[2];" ~

        "out gl_PerVertex {" ~
          "highp vec4 gl_Position;" ~
          "highp float gl_PointSize;" ~
        "};" ~
        "\n"
      );
    }

    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 140) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 310)) {
      stageBuiltins.STAGE_TESSEVALUATION ~= (
        "in highp int gl_DeviceIndex;" ~
        "in highp int gl_ViewIndex;" ~
        "\n"
      );
    }

    if (profile != EProfile(ES_PROFILE: 1)) {
      stageBuiltins.STAGE_FRAGMENT ~= (
        "vec4 gl_FragCoord;" ~
        "bool gl_FrontFacing;" ~
        "float gl_FragDepth;"
      );
      if (version_ >= 120)
        stageBuiltins.STAGE_FRAGMENT ~=
          "vec2 gl_PointCoord;";
      if (version_ >= 140)
        stageBuiltins.STAGE_FRAGMENT ~=
          "out int gl_FragStencilRefARB;";
      if (IncludeLegacy(version_, profile, spvVersion) || (!ForwardCompatibility && version_ < 420))
        stageBuiltins.STAGE_FRAGMENT ~=
          "vec4 gl_FragColor;";
      
      if (version_ < 130) {
        stageBuiltins.STAGE_FRAGMENT ~= (
          "varying vec4 gl_Color;" ~
          "varying vec4 gl_SecondaryColor;" ~
          "varying vec4 gl_TexCoord[];" ~
          "varying float gl_FogFragCoord;"
        );
      } else {
        stageBuiltins.STAGE_FRAGMENT ~=
          "in float gl_ClipDistance[];";

        if (IncludeLegacy(version_, profile, spvVersion)) {
          if (version_ < 150)
            stageBuiltins.STAGE_FRAGMENT ~= (
              "in float gl_FogFragCoord;" ~
              "in vec4 gl_TexCoord[];" ~
              "in vec4 gl_Color;" ~
              "in vec4 gl_SecondaryColor;"
            );
          else
            stageBuiltins.STAGE_FRAGMENT ~= (
              "in gl_PerFragment {" ~
                "in float gl_FogFragCoord;" ~
                "in vec4 gl_TexCoord[];" ~
                "in vec4 gl_Color;" ~
                "in vec4 gl_SecondaryColor;" ~
              "};"
            );
        }
      }

      if (version_ >= 150)
        stageBuiltins.STAGE_FRAGMENT ~=
          "flat in int gl_PrimitiveID;";
      
      if (version_ >= 130) {
        stageBuiltins.STAGE_FRAGMENT ~= (
          "flat in int gl_SampleID;" ~
          "in vec2 gl_SamplePosition;" ~
          "out int gl_SampleMask[];"
        );

        if (spvVersion.spv == 0)
          stageBuiltins.STAGE_FRAGMENT ~=
            "uniform int gl_NumSamples;";
      }

      if (version_ >= 150)
        stageBuiltins.STAGE_FRAGMENT ~=
          "flat in int gl_SampleMaskIn[];";

      if (version_ >= 430)
        stageBuiltins.STAGE_FRAGMENT ~= (
          "flat in int gl_Layer;" ~
          "flat in int gl_ViewportIndex;"
        );

      if (version_ >= 450)
        stageBuiltins.STAGE_FRAGMENT ~= (
          "in float gl_CullDistance[];" ~
          "bool gl_HelperInvocation;" ~

          "flat in ivec2 gl_FragSizeEXT;" ~
          "flat in int gl_FragInvocationCountEXT;" ~

          "in vec2 gl_BaryCoordNoPerspAMD;" ~
          "in vec2 gl_BaryCoordNoPerspCentroidAMD;" ~
          "in vec2 gl_BaryCoordNoPerspSampleAMD;" ~
          "in vec2 gl_BaryCoordSmoothAMD;" ~
          "in vec2 gl_BaryCoordSmoothCentroidAMD;" ~
          "in vec2 gl_BaryCoordSmoothSampleAMD;" ~
          "in vec3 gl_BaryCoordPullModelAMD;"
        );

      if (version_ >= 430)
        stageBuiltins.STAGE_FRAGMENT ~=
          "in bool gl_FragFullyCoveredNV;";

      if (version_ >= 450)
        stageBuiltins.STAGE_FRAGMENT ~= (
          "flat in ivec2 gl_FragmentSizeNV;" ~
          "flat in int gl_InvocationsPerPixelNV;" ~
          "in vec3 gl_BaryCoordNV;" ~
          "in vec3 gl_BaryCoordNoPerspNV;" ~
          "in vec3 gl_BaryCoordEXT;" ~
          "in vec3 gl_BaryCoordNoPerspEXT;" ~

          "flat in int gl_ShadingRateEXT;"
        );
    } else {
      if (version_ == 100) {
        stageBuiltins.STAGE_FRAGMENT ~= (
          "mediump vec4 gl_FragCoord;" ~
          "bool gl_FrontFacing;" ~
          "mediump vec4 gl_FragColor;" ~
          "mediump vec2 gl_PointCoord;"
        );
      }
      if (version_ >= 300) {
        stageBuiltins.STAGE_FRAGMENT ~= (
          "highp vec4 gl_FragCoord;" ~
          "bool gl_FrontFacing;" ~
          "mediump vec2 gl_PointCoord;" ~
          "highp float gl_FragDepth;"
        );
      }
      if (version_ >= 310) {
        stageBuiltins.STAGE_FRAGMENT ~= (
          "bool gl_HelperInvocation;" ~
          "flat in highp int gl_PrimitiveID;" ~
          "flat in highp int gl_Layer;" ~

          "flat in lowp int gl_SampleID;" ~
          "in mediump vec2 gl_SamplePosition;" ~
          "flat in highp int gl_SampleMaskIn[];" ~
          "out highp int gl_SampleMask[];"
        );

        if (spvVersion.spv == 0)
          stageBuiltins.STAGE_FRAGMENT ~=
            "uniform lowp int gl_NumSamples;";
      }
      stageBuiltins.STAGE_FRAGMENT ~=
        "highp float gl_FragDepthEXT;";

      if (version_ >= 310)
        stageBuiltins.STAGE_FRAGMENT ~= (
          "flat in ivec2 gl_FragSizeEXT;" ~
          "flat in int gl_FragInvocationCountEXT;"
        );
      if (version_ >= 320)
        stageBuiltins.STAGE_FRAGMENT ~= (
          "flat in ivec2 gl_FragmentSizeNV;" ~
          "flat in int gl_InvocationsPerPixelNV;" ~
        
          "in vec3 gl_BaryCoordNV;" ~
          "in vec3 gl_BaryCoordNoPerspNV;" ~
          "in vec3 gl_BaryCoordEXT;" ~
          "in vec3 gl_BaryCoordNoPerspEXT;"
        );
      if (version_ >= 310)
        stageBuiltins.STAGE_FRAGMENT ~=
          "flat in highp int gl_ShadingRateEXT;";
    }
    
    stageBuiltins.STAGE_FRAGMENT ~= "\n";

    if (version_ >= 130)
      add2ndGenerationSamplingImaging(version_, profile, spvVersion);

    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 140) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 310)) {
      stageBuiltins.STAGE_FRAGMENT.insert(
        "flat in highp int gl_DeviceIndex;" ~
        "flat in highp int gl_ViewIndex;" ~
        "\n");
    }

    if (version_ >= 300 /* both ES and non-ES */) {
      stageBuiltins.STAGE_FRAGMENT.insert(
        "flat in highp uint gl_ViewID_OVR;" ~
        "\n");
    }
    
    if ((profile == EProfile(ES_PROFILE: 1) && version_ >= 310) ||
      (profile != EProfile(ES_PROFILE: 1) && version_ >= 460)) {
      stageBuiltins.STAGE_FRAGMENT.insert(
        "flat in highp uvec2 gl_TileOffsetQCOM;" ~
        "flat in highp uvec3 gl_TileDimensionQCOM;" ~
        "flat in highp uvec2 gl_TileApronSizeQCOM;" ~
        "\n");
    }

    
    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 450) {
      string ballotDecls =
        "uniform uint gl_SubGroupSizeARB;" ~
        "in uint gl_SubGroupInvocationARB;" ~
        "in uint64_t gl_SubGroupEqMaskARB;" ~
        "in uint64_t gl_SubGroupGeMaskARB;" ~
        "in uint64_t gl_SubGroupGtMaskARB;" ~
        "in uint64_t gl_SubGroupLeMaskARB;" ~
        "in uint64_t gl_SubGroupLtMaskARB;" ~
        "\n";
      string rtBallotDecls =
        "uniform volatile uint gl_SubGroupSizeARB;" ~
        "in volatile uint gl_SubGroupInvocationARB;" ~
        "in volatile uint64_t gl_SubGroupEqMaskARB;" ~
        "in volatile uint64_t gl_SubGroupGeMaskARB;" ~
        "in volatile uint64_t gl_SubGroupGtMaskARB;" ~
        "in volatile uint64_t gl_SubGroupLeMaskARB;" ~
        "in volatile uint64_t gl_SubGroupLtMaskARB;" ~
        "\n";
      string fragmentBallotDecls =
        "uniform uint gl_SubGroupSizeARB;" ~
        "flat in uint gl_SubGroupInvocationARB;" ~
        "flat in uint64_t gl_SubGroupEqMaskARB;" ~
        "flat in uint64_t gl_SubGroupGeMaskARB;" ~
        "flat in uint64_t gl_SubGroupGtMaskARB;" ~
        "flat in uint64_t gl_SubGroupLeMaskARB;" ~
        "flat in uint64_t gl_SubGroupLtMaskARB;" ~
        "\n";
      stageBuiltins.STAGE_VERTEX.insert(ballotDecls);
      stageBuiltins.STAGE_TESSCONTROL.insert(ballotDecls);
      stageBuiltins.STAGE_TESSEVALUATION.insert(ballotDecls);
      stageBuiltins.STAGE_GEOMETRY.insert(ballotDecls);
      stageBuiltins.STAGE_COMPUTE.insert(ballotDecls);
      stageBuiltins.STAGE_FRAGMENT.insert(fragmentBallotDecls);
      stageBuiltins.STAGE_MESH.insert(ballotDecls);
      stageBuiltins.STAGE_TASK.insert(ballotDecls);
      stageBuiltins.STAGE_RAYGEN.insert(rtBallotDecls);
      stageBuiltins.STAGE_INTERSECT.insert(rtBallotDecls);
      
      stageBuiltins.STAGE_ANYHIT.insert(ballotDecls);
      stageBuiltins.STAGE_CLOSESTHIT.insert(rtBallotDecls);
      stageBuiltins.STAGE_MISS.insert(rtBallotDecls);
      stageBuiltins.STAGE_CALLABLE.insert(rtBallotDecls);
    }

    
    if ((profile == EProfile(ES_PROFILE: 1) && version_ >= 310) ||
      (profile != EProfile(ES_PROFILE: 1) && version_ >= 140)) {
      string subgroupDecls =
        "in mediump uint gl_SubgroupSize;" ~
        "in mediump uint gl_SubgroupInvocationID;" ~
        "in highp uvec4 gl_SubgroupEqMask;" ~
        "in highp uvec4 gl_SubgroupGeMask;" ~
        "in highp uvec4 gl_SubgroupGtMask;" ~
        "in highp uvec4 gl_SubgroupLeMask;" ~
        "in highp uvec4 gl_SubgroupLtMask;" ~
        
        "in highp uint gl_WarpsPerSMNV;" ~
        "in highp uint gl_SMCountNV;" ~
        "in highp uint gl_WarpIDNV;" ~
        "in highp uint gl_SMIDNV;" ~
        
        "in highp uint gl_CoreIDARM;" ~
        "in highp uint gl_CoreCountARM;" ~
        "in highp uint gl_CoreMaxIDARM;" ~
        "in highp uint gl_WarpIDARM;" ~
        "in highp uint gl_WarpMaxIDARM;" ~
        "\n";
      string fragmentSubgroupDecls =
        "flat in mediump uint gl_SubgroupSize;" ~
        "flat in mediump uint gl_SubgroupInvocationID;" ~
        "flat in highp uvec4 gl_SubgroupEqMask;" ~
        "flat in highp uvec4 gl_SubgroupGeMask;" ~
        "flat in highp uvec4 gl_SubgroupGtMask;" ~
        "flat in highp uvec4 gl_SubgroupLeMask;" ~
        "flat in highp uvec4 gl_SubgroupLtMask;" ~
        
        "flat in highp uint gl_WarpsPerSMNV;" ~
        "flat in highp uint gl_SMCountNV;" ~
        "flat in highp uint gl_WarpIDNV;" ~
        "flat in highp uint gl_SMIDNV;" ~
        
        "flat in highp uint gl_CoreIDARM;" ~
        "flat in highp uint gl_CoreCountARM;" ~
        "flat in highp uint gl_CoreMaxIDARM;" ~
        "flat in highp uint gl_WarpIDARM;" ~
        "flat in highp uint gl_WarpMaxIDARM;" ~
        "\n";
      string computeSubgroupDecls =
        "in highp uint gl_NumSubgroups;" ~
        "in highp uint gl_SubgroupID;" ~
        "\n";
        
      string rtSubgroupDecls =
        "in mediump volatile uint gl_SubgroupSize;" ~
        "in mediump volatile uint gl_SubgroupInvocationID;" ~
        "in highp volatile uvec4 gl_SubgroupEqMask;" ~
        "in highp volatile uvec4 gl_SubgroupGeMask;" ~
        "in highp volatile uvec4 gl_SubgroupGtMask;" ~
        "in highp volatile uvec4 gl_SubgroupLeMask;" ~
        "in highp volatile uvec4 gl_SubgroupLtMask;" ~
        
        "in highp uint gl_WarpsPerSMNV;" ~
        "in highp uint gl_SMCountNV;" ~
        "in highp volatile uint gl_WarpIDNV;" ~
        "in highp volatile uint gl_SMIDNV;" ~
        
        "in highp uint gl_CoreIDARM;" ~
        "in highp uint gl_CoreCountARM;" ~
        "in highp uint gl_CoreMaxIDARM;" ~
        "in highp uint gl_WarpIDARM;" ~
        "in highp uint gl_WarpMaxIDARM;" ~
        "\n";

      stageBuiltins.STAGE_VERTEX.insert(subgroupDecls);
      stageBuiltins.STAGE_TESSCONTROL.insert(subgroupDecls);
      stageBuiltins.STAGE_TESSEVALUATION.insert(subgroupDecls);
      stageBuiltins.STAGE_GEOMETRY.insert(subgroupDecls);
      stageBuiltins.STAGE_COMPUTE.insert(subgroupDecls);
      stageBuiltins.STAGE_COMPUTE.insert(computeSubgroupDecls);
      stageBuiltins.STAGE_FRAGMENT.insert(fragmentSubgroupDecls);
      stageBuiltins.STAGE_MESH.insert(subgroupDecls);
      stageBuiltins.STAGE_MESH.insert(computeSubgroupDecls);
      stageBuiltins.STAGE_TASK.insert(subgroupDecls);
      stageBuiltins.STAGE_TASK.insert(computeSubgroupDecls);
      stageBuiltins.STAGE_RAYGEN.insert(rtSubgroupDecls);
      stageBuiltins.STAGE_INTERSECT.insert(rtSubgroupDecls);
      stageBuiltins.STAGE_ANYHIT.insert(subgroupDecls);
      stageBuiltins.STAGE_CLOSESTHIT.insert(rtSubgroupDecls);
      stageBuiltins.STAGE_MISS.insert(rtSubgroupDecls);
      stageBuiltins.STAGE_CALLABLE.insert(rtSubgroupDecls);
    }

    
    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 460) {
      string constRayFlags =
        "const uint gl_RayFlagsNoneNV = 0U;" ~
        "const uint gl_RayFlagsNoneEXT = 0U;" ~
        "const uint gl_RayFlagsOpaqueNV = 1U;" ~
        "const uint gl_RayFlagsOpaqueEXT = 1U;" ~
        "const uint gl_RayFlagsNoOpaqueNV = 2U;" ~
        "const uint gl_RayFlagsNoOpaqueEXT = 2U;" ~
        "const uint gl_RayFlagsTerminateOnFirstHitNV = 4U;" ~
        "const uint gl_RayFlagsTerminateOnFirstHitEXT = 4U;" ~
        "const uint gl_RayFlagsSkipClosestHitShaderNV = 8U;" ~
        "const uint gl_RayFlagsSkipClosestHitShaderEXT = 8U;" ~
        "const uint gl_RayFlagsCullBackFacingTrianglesNV = 16U;" ~
        "const uint gl_RayFlagsCullBackFacingTrianglesEXT = 16U;" ~
        "const uint gl_RayFlagsCullFrontFacingTrianglesNV = 32U;" ~
        "const uint gl_RayFlagsCullFrontFacingTrianglesEXT = 32U;" ~
        "const uint gl_RayFlagsCullOpaqueNV = 64U;" ~
        "const uint gl_RayFlagsCullOpaqueEXT = 64U;" ~
        "const uint gl_RayFlagsCullNoOpaqueNV = 128U;" ~
        "const uint gl_RayFlagsCullNoOpaqueEXT = 128U;" ~
        "const uint gl_RayFlagsSkipTrianglesEXT = 256U;" ~
        "const uint gl_RayFlagsSkipBuiltinPrimitivesNV = 256U;" ~
        "const uint gl_RayFlagsSkipAABBEXT = 512U;" ~
        "const uint gl_RayFlagsForceOpacityMicromap2StateEXT = 1024U;" ~
        "const uint gl_HitKindFrontFacingTriangleEXT = 254U;" ~
        "const uint gl_HitKindBackFacingTriangleEXT = 255U;" ~
        "in uint gl_HitKindFrontFacingMicroTriangleNV;" ~
        "in uint gl_HitKindBackFacingMicroTriangleNV;" ~
        "const int gl_ClusterIDNoneNV = -1;" ~
        "\n";

      string constRayQueryIntersection =
        "const uint gl_RayQueryCandidateIntersectionEXT = 0U;" ~
        "const uint gl_RayQueryCommittedIntersectionEXT = 1U;" ~
        "const uint gl_RayQueryCommittedIntersectionNoneEXT = 0U;" ~
        "const uint gl_RayQueryCommittedIntersectionTriangleEXT = 1U;" ~
        "const uint gl_RayQueryCommittedIntersectionGeneratedEXT = 2U;" ~
        "const uint gl_RayQueryCandidateIntersectionTriangleEXT = 0U;" ~
        "const uint gl_RayQueryCandidateIntersectionAABBEXT = 1U;" ~
        "\n";

      string rayGenDecls =
        "in uvec3 gl_LaunchIDNV;" ~
        "in uvec3 gl_LaunchIDEXT;" ~
        "in uvec3 gl_LaunchSizeNV;" ~
        "in uvec3 gl_LaunchSizeEXT;" ~
        "\n";
      string intersectDecls =
        "in uvec3 gl_LaunchIDNV;" ~
        "in uvec3 gl_LaunchIDEXT;" ~
        "in uvec3 gl_LaunchSizeNV;" ~
        "in uvec3 gl_LaunchSizeEXT;" ~
        "in int gl_PrimitiveID;" ~
        "in int gl_InstanceID;" ~
        "in int gl_InstanceCustomIndexNV;" ~
        "in int gl_InstanceCustomIndexEXT;" ~
        "in int gl_GeometryIndexEXT;" ~
        "in vec3 gl_WorldRayOriginNV;" ~
        "in vec3 gl_WorldRayOriginEXT;" ~
        "in vec3 gl_WorldRayDirectionNV;" ~
        "in vec3 gl_WorldRayDirectionEXT;" ~
        "in vec3 gl_ObjectRayOriginNV;" ~
        "in vec3 gl_ObjectRayOriginEXT;" ~
        "in vec3 gl_ObjectRayDirectionNV;" ~
        "in vec3 gl_ObjectRayDirectionEXT;" ~
        "in float gl_RayTminNV;" ~
        "in float gl_RayTminEXT;" ~
        "in float gl_RayTmaxNV;" ~
        "in volatile float gl_RayTmaxEXT;" ~
        "in mat4x3 gl_ObjectToWorldNV;" ~
        "in mat4x3 gl_ObjectToWorldEXT;" ~
        "in mat3x4 gl_ObjectToWorld3x4EXT;" ~
        "in mat4x3 gl_WorldToObjectNV;" ~
        "in mat4x3 gl_WorldToObjectEXT;" ~
        "in mat3x4 gl_WorldToObject3x4EXT;" ~
        "in uint gl_IncomingRayFlagsNV;" ~
        "in uint gl_IncomingRayFlagsEXT;" ~
        "in float gl_CurrentRayTimeNV;" ~
        "in uint gl_CullMaskEXT;" ~
        "\n";
      string hitDecls =
        "in uvec3 gl_LaunchIDNV;" ~
        "in uvec3 gl_LaunchIDEXT;" ~
        "in uvec3 gl_LaunchSizeNV;" ~
        "in uvec3 gl_LaunchSizeEXT;" ~
        "in int gl_PrimitiveID;" ~
        "in int gl_InstanceID;" ~
        "in int gl_InstanceCustomIndexNV;" ~
        "in int gl_InstanceCustomIndexEXT;" ~
        "in int gl_GeometryIndexEXT;" ~
        "in vec3 gl_WorldRayOriginNV;" ~
        "in vec3 gl_WorldRayOriginEXT;" ~
        "in vec3 gl_WorldRayDirectionNV;" ~
        "in vec3 gl_WorldRayDirectionEXT;" ~
        "in vec3 gl_ObjectRayOriginNV;" ~
        "in vec3 gl_ObjectRayOriginEXT;" ~
        "in vec3 gl_ObjectRayDirectionNV;" ~
        "in vec3 gl_ObjectRayDirectionEXT;" ~
        "in float gl_RayTminNV;" ~
        "in float gl_RayTminEXT;" ~
        "in float gl_RayTmaxNV;" ~
        "in float gl_RayTmaxEXT;" ~
        "in float gl_HitTNV;" ~
        "in float gl_HitTEXT;" ~
        "in uint gl_HitKindNV;" ~
        "in uint gl_HitKindEXT;" ~
        "in mat4x3 gl_ObjectToWorldNV;" ~
        "in mat4x3 gl_ObjectToWorldEXT;" ~
        "in mat3x4 gl_ObjectToWorld3x4EXT;" ~
        "in mat4x3 gl_WorldToObjectNV;" ~
        "in mat4x3 gl_WorldToObjectEXT;" ~
        "in mat3x4 gl_WorldToObject3x4EXT;" ~
        "in uint gl_IncomingRayFlagsNV;" ~
        "in uint gl_IncomingRayFlagsEXT;" ~
        "in float gl_CurrentRayTimeNV;" ~
        "in uint gl_CullMaskEXT;" ~
        "in vec3 gl_HitTriangleVertexPositionsEXT[3];" ~
        "in vec3 gl_HitMicroTriangleVertexPositionsNV[3];" ~
        "in vec2 gl_HitMicroTriangleVertexBarycentricsNV[3];" ~
        "in int gl_ClusterIDNV;" ~
        "in bool gl_HitIsSphereNV;" ~
        "in bool gl_HitIsLSSNV;" ~
        "in vec3 gl_HitSpherePositionNV;" ~
        "in float gl_HitSphereRadiusNV;" ~
        "in vec3 gl_HitLSSPositionsNV[2];" ~
        "in float gl_HitLSSRadiiNV[2];" ~
        "\n";

      string missDecls =
        "in uvec3 gl_LaunchIDNV;" ~
        "in uvec3 gl_LaunchIDEXT;" ~
        "in uvec3 gl_LaunchSizeNV;" ~
        "in uvec3 gl_LaunchSizeEXT;" ~
        "in vec3 gl_WorldRayOriginNV;" ~
        "in vec3 gl_WorldRayOriginEXT;" ~
        "in vec3 gl_WorldRayDirectionNV;" ~
        "in vec3 gl_WorldRayDirectionEXT;" ~
        "in vec3 gl_ObjectRayOriginNV;" ~
        "in vec3 gl_ObjectRayDirectionNV;" ~
        "in float gl_RayTminNV;" ~
        "in float gl_RayTminEXT;" ~
        "in float gl_RayTmaxNV;" ~
        "in float gl_RayTmaxEXT;" ~
        "in uint gl_IncomingRayFlagsNV;" ~
        "in uint gl_IncomingRayFlagsEXT;" ~
        "in float gl_CurrentRayTimeNV;" ~
        "in uint gl_CullMaskEXT;" ~
        "\n";

      string callableDecls =
        "in uvec3 gl_LaunchIDNV;" ~
        "in uvec3 gl_LaunchIDEXT;" ~
        "in uvec3 gl_LaunchSizeNV;" ~
        "in uvec3 gl_LaunchSizeEXT;" ~
        "\n";


      commonBuiltins.insert(constRayQueryIntersection);
      commonBuiltins.insert(constRayFlags);

      stageBuiltins.STAGE_RAYGEN.insert(rayGenDecls);
      stageBuiltins.STAGE_INTERSECT.insert(intersectDecls);
      stageBuiltins.STAGE_ANYHIT.insert(hitDecls);
      stageBuiltins.STAGE_CLOSESTHIT.insert(hitDecls);
      stageBuiltins.STAGE_MISS.insert(missDecls);
      stageBuiltins.STAGE_CALLABLE.insert(callableDecls);
    }

    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 140)) {
      string deviceIndex =
        "in highp int gl_DeviceIndex;" ~
        "\n";

      stageBuiltins.STAGE_RAYGEN.insert(deviceIndex);
      stageBuiltins.STAGE_INTERSECT.insert(deviceIndex);
      stageBuiltins.STAGE_ANYHIT.insert(deviceIndex);
      stageBuiltins.STAGE_CLOSESTHIT.insert(deviceIndex);
      stageBuiltins.STAGE_MISS.insert(deviceIndex);
    }

    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 420) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 310)) {
      commonBuiltins.insert("const int gl_ScopeDevice = 1;\n");
      commonBuiltins.insert("const int gl_ScopeWorkgroup = 2;\n");
      commonBuiltins.insert("const int gl_ScopeSubgroup = 3;\n");
      commonBuiltins.insert("const int gl_ScopeInvocation = 4;\n");
      commonBuiltins.insert("const int gl_ScopeQueueFamily = 5;\n");
      commonBuiltins.insert("const int gl_ScopeShaderCallEXT = 6;\n");

      commonBuiltins.insert("const int gl_SemanticsRelaxed = 0x0;\n");
      commonBuiltins.insert("const int gl_SemanticsAcquire = 0x2;\n");
      commonBuiltins.insert("const int gl_SemanticsRelease = 0x4;\n");
      commonBuiltins.insert("const int gl_SemanticsAcquireRelease = 0x8;\n");
      commonBuiltins.insert("const int gl_SemanticsMakeAvailable = 0x2000;\n");
      commonBuiltins.insert("const int gl_SemanticsMakeVisible = 0x4000;\n");
      commonBuiltins.insert("const int gl_SemanticsVolatile = 0x8000;\n");

      commonBuiltins.insert("const int gl_StorageSemanticsNone = 0x0;\n");
      commonBuiltins.insert("const int gl_StorageSemanticsBuffer = 0x40;\n");
      commonBuiltins.insert("const int gl_StorageSemanticsShared = 0x100;\n");
      commonBuiltins.insert("const int gl_StorageSemanticsImage = 0x800;\n");
      commonBuiltins.insert("const int gl_StorageSemanticsOutput = 0x1000;\n");
    }

    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 450) || (profile == EProfile(ES_PROFILE: 1) && version_ >= 310)) {
      static foreach (i; 0..stageBuiltins.tupleof.length) {
        stageBuiltins.tupleof[i].insert("const highp int gl_ShadingRateFlag2VerticalPixelsEXT = 1;\n");
        stageBuiltins.tupleof[i].insert("const highp int gl_ShadingRateFlag4VerticalPixelsEXT = 2;\n");
        stageBuiltins.tupleof[i].insert("const highp int gl_ShadingRateFlag2HorizontalPixelsEXT = 4;\n");
        stageBuiltins.tupleof[i].insert("const highp int gl_ShadingRateFlag4HorizontalPixelsEXT = 8;\n");
      }
    }
    
    
    if ((profile != EProfile(ES_PROFILE: 1) && version_ >= 420) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 310)) {
      for (int ms = 0; ms <= 1; ++ms) { 
        for (int arrayed = 0; arrayed <= 1; ++arrayed) { 
          for (TSamplerDim dim = TSamplerDim.Esd1D; dim < TSamplerDim.EsdSubpass; ++dim) { 
            if ((dim == TSamplerDim.Esd1D || dim == TSamplerDim.EsdRect) && profile == EProfile(ES_PROFILE: 1))
              continue;
                    
            if ((dim == TSamplerDim.Esd3D || dim == TSamplerDim.EsdRect || dim == TSamplerDim.EsdBuffer) && arrayed)
              continue;
                    
            if (dim != TSamplerDim.Esd2D && ms)
              continue;
                    
                    
            foreach (bType; [TBasicType.EbtInt64, TBasicType.EbtUint64]) {
              TSampler sampler;
          
              sampler.setImage(bType, dim, arrayed ? true : false, false, ms ? true : false);

              string typeName = sampler.getString();
              addQueryFunctions(sampler, typeName, version_, profile);
              addImageFunctions(sampler, typeName, version_, profile);
            }
          }
        }
      }
    }
  }

  override void initialize(
    int version_, EProfile, in SpvVersion spvVersion, EShLanguage
  ) {
    throw new Exception("unimplemented");
  }

  void add2ndGenerationSamplingImaging(
    int version_, EProfile profile, in SpvVersion spvVersion
  ) {
    enum TBasicType[] bTypes = [
      TBasicType.EbtFloat, TBasicType.EbtInt, TBasicType.EbtUint, TBasicType.EbtFloat16
    ];
    bool skipBuffer = (profile == EProfile(ES_PROFILE: 1) && version_ < 310) || (profile != EProfile(ES_PROFILE: 1) && version_ < 140);
    bool skipCubeArrayed = (profile == EProfile(ES_PROFILE: 1) && version_ < 310) || (profile != EProfile(ES_PROFILE: 1) && version_ < 130);
    for (int image = 0; image <= 1; ++image) {
      for (int shadow = 0; shadow <= 1; ++shadow) {
        for (int ms = 0; ms <= 1; ++ms) {
          if ((ms || image) && shadow)
            continue;
          if (ms && profile != EProfile(ES_PROFILE: 1) && version_ < 140)
            continue;
          if (ms && image && profile == EProfile(ES_PROFILE: 1))
            continue;
          if (ms && profile == EProfile(ES_PROFILE: 1) && version_ < 310)
            continue;

          for (int arrayed = 0; arrayed <= 1; ++arrayed) {
            for (TSamplerDim dim = TSamplerDim.Esd1D; dim < TSamplerDim.EsdNumDims; ++dim) {
              if (dim == TSamplerDim.EsdAttachmentEXT)
                  continue;
              if (dim == TSamplerDim.EsdSubpass && spvVersion.vulkan == 0)
                  continue;
              if (dim == TSamplerDim.EsdSubpass && (image || shadow || arrayed))
                  continue;
              if ((dim == TSamplerDim.Esd1D || dim == TSamplerDim.EsdRect) && profile == EProfile(ES_PROFILE: 1))
                  continue;
              if (dim == TSamplerDim.EsdSubpass && spvVersion.vulkan == 0)
                  continue;
              if (dim == TSamplerDim.EsdSubpass && (image || shadow || arrayed))
                  continue;
              if ((dim == TSamplerDim.Esd1D || dim == TSamplerDim.EsdRect) && profile == EProfile(ES_PROFILE: 1))
                  continue;
              if (dim != TSamplerDim.Esd2D && dim != TSamplerDim.EsdSubpass && ms)
                  continue;
              if (dim == TSamplerDim.EsdBuffer && skipBuffer)
                  continue;
              if (dim == TSamplerDim.EsdBuffer && (shadow || arrayed || ms))
                  continue;
              if (ms && arrayed && profile == EProfile(ES_PROFILE: 1) && version_ < 310)
                  continue;
              if (dim == TSamplerDim.Esd3D && shadow)
                  continue;
              if (dim == TSamplerDim.EsdCube && arrayed && skipCubeArrayed)
                  continue;
              if ((dim == TSamplerDim.Esd3D || dim == TSamplerDim.EsdRect) && arrayed)
                  continue;

              for (size_t bType = 0; bType < bTypes.length; ++bType) {
                if (bTypes[bType] == TBasicType.EbtFloat16 && (profile == EProfile(ES_PROFILE: 1) || version_ < 450))
                  continue;
                if (dim == TSamplerDim.EsdRect && version_ < 140 && bType > 0)
                  continue;
                if (shadow && (bTypes[bType] == TBasicType.EbtInt || bTypes[bType] == TBasicType.EbtUint))
                  continue;

                TSampler sampler;
                if (dim == TSamplerDim.EsdSubpass) {
                  sampler.setSubpass(bTypes[bType], ms ? true : false);
                } else if (dim == TSamplerDim.EsdAttachmentEXT) {
                  sampler.setAttachmentEXT(bTypes[bType]);
                } else if (image) {
                  sampler.setImage(
                    bTypes[bType],
                    dim,
                    arrayed ? true : false,
                    shadow ? true : false,
                    ms ? true : false);
                } else {
                  sampler.set(
                    bTypes[bType],
                    dim,
                    arrayed ? true : false,
                    shadow ? true : false,
                    ms ? true : false);
                }

                string typeName = sampler.getString;

                if (dim == TSamplerDim.EsdSubpass) {
                  addSubpassSampling(sampler, typeName, version_, profile);
                  continue;
                }

                addQueryFunctions(sampler, typeName, version_, profile);

                if (image)
                  addImageFunctions(sampler, typeName, version_, profile);
                else {
                  addSamplingFunctions(sampler, typeName, version_, profile);
                  addGatherFunctions(sampler, typeName, version_, profile);
                  if (spvVersion.vulkan > 0 && sampler.isCombined() && !sampler.shadow) {
                    sampler.setTexture(
                      sampler.type,
                      sampler.dim,
                      sampler.arrayed,
                      sampler.shadow,
                      sampler.ms);
                    string textureTypeName = sampler.getString;
                    addSamplingFunctions(sampler, textureTypeName, version_, profile);
                    addQueryFunctions(sampler, textureTypeName, version_, profile);
                  }
                }
              }
            }
          }
        }
      }
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 450) {
      commonBuiltins ~= `
        bool sparseTexelsResidentARB(int code);
      `.outdent;
    }
  }

  void addSubpassSampling(TSampler sampler, string typeName, int, EProfile) {
    stageBuiltins.STAGE_FRAGMENT.insert(prefixes[sampler.type]);
    stageBuiltins.STAGE_FRAGMENT.insert("vec4 subpassLoad");
    stageBuiltins.STAGE_FRAGMENT.insert("(");
    stageBuiltins.STAGE_FRAGMENT.insert(typeName);
    if (sampler.isMultiSample()) stageBuiltins.STAGE_FRAGMENT.insert(", int");
    stageBuiltins.STAGE_FRAGMENT.insert(");\n");
  }

  void addImageFunctions(TSampler sampler, string typeName, int version_, EProfile profile) {
    int dims = dimMap[sampler.dim];
    if (sampler.arrayed && sampler.dim != TSamplerDim.EsdCube)
      ++dims;

    DList!string imageParams = DList!string([typeName]);
    if (dims == 1)
      imageParams.insert(", int");
    else {
      imageParams.insert(", ivec");
      imageParams.insert(postfixes[dims]);
    }
    if (sampler.isMultiSample())
      imageParams.insert(", int");

    if (profile == EProfile(ES_PROFILE: 1))
      commonBuiltins.insert("highp ");
    commonBuiltins.insert(prefixes[sampler.type]);
    commonBuiltins.insert("vec4 imageLoad(readonly volatile coherent nontemporal ");
    commonBuiltins.insert(imageParams[]);
    commonBuiltins.insert(");\n");

    commonBuiltins.insert("void imageStore(writeonly volatile coherent nontemporal ");
    commonBuiltins.insert(imageParams[]);
    commonBuiltins.insert(", ");
    commonBuiltins.insert(prefixes[sampler.type]);
    commonBuiltins.insert("vec4);\n");

    if (!sampler.is1D() && !sampler.isBuffer() && profile != EProfile(ES_PROFILE: 1) && version_ >= 450) {
      commonBuiltins.insert("int sparseImageLoadARB(readonly volatile coherent nontemporal ");
      commonBuiltins.insert(imageParams[]);
      commonBuiltins.insert(", out ");
      commonBuiltins.insert(prefixes[sampler.type]);
      commonBuiltins.insert("vec4");
      commonBuiltins.insert(");\n");
    }

    if (profile != EProfile(ES_PROFILE: 1) ||
      (profile == EProfile(ES_PROFILE: 1) && version_ >= 310)) {
      if (sampler.type == TBasicType.EbtInt || sampler.type == TBasicType.EbtUint || sampler.type == TBasicType.EbtInt64 || sampler.type == TBasicType.EbtUint64 ) {
        string dataType;
        switch (sampler.type) {
          case(TBasicType.EbtInt): dataType = "highp int"; break;
          case(TBasicType.EbtUint): dataType = "highp uint"; break;
          case(TBasicType.EbtInt64): dataType = "highp int64_t"; break;
          case(TBasicType.EbtUint64): dataType = "highp uint64_t"; break;
          default: dataType = "";
        }

        const int numBuiltins = 7;

        static string[numBuiltins] atomicFunc = [
          " imageAtomicAdd(volatile coherent nontemporal ",
          " imageAtomicMin(volatile coherent nontemporal ",
          " imageAtomicMax(volatile coherent nontemporal ",
          " imageAtomicAnd(volatile coherent nontemporal ",
          " imageAtomicOr(volatile coherent nontemporal ",
          " imageAtomicXor(volatile coherent nontemporal ",
          " imageAtomicExchange(volatile coherent nontemporal "
        ];

        for (int j = 0; j < 2; ++j) {
          for (size_t i = 0; i < numBuiltins; ++i) {
            commonBuiltins.insert(dataType);
            commonBuiltins.insert(atomicFunc[i]);
            commonBuiltins.insert(imageParams[]);
            commonBuiltins.insert(", ");
            commonBuiltins.insert(dataType);
            if (j == 1) {
              commonBuiltins.insert(", int, int, int");
            }
            commonBuiltins.insert(");\n");
          }

          commonBuiltins.insert(dataType);
          commonBuiltins.insert(" imageAtomicCompSwap(volatile coherent nontemporal ");
          commonBuiltins.insert(imageParams[]);
          commonBuiltins.insert(", ");
          commonBuiltins.insert(dataType);
          commonBuiltins.insert(", ");
          commonBuiltins.insert(dataType);
          if (j == 1) {
            commonBuiltins.insert(", int, int, int, int, int");
          }
          commonBuiltins.insert(");\n");
        }

        commonBuiltins.insert(dataType);
        commonBuiltins.insert(" imageAtomicLoad(volatile coherent nontemporal ");
        commonBuiltins.insert(imageParams[]);
        commonBuiltins.insert(", int, int, int);\n");

        commonBuiltins.insert("void imageAtomicStore(volatile coherent nontemporal ");
        commonBuiltins.insert(imageParams[]);
        commonBuiltins.insert(", ");
        commonBuiltins.insert(dataType);
        commonBuiltins.insert(", int, int, int);\n");
      } else {
        if (profile == EProfile(ES_PROFILE: 1) && version_ >= 310) {
          commonBuiltins.insert("float imageAtomicExchange(volatile coherent nontemporal ");
          commonBuiltins.insert(imageParams[]);
          commonBuiltins.insert(", float);\n");
        }

        if (profile != EProfile(ES_PROFILE: 1) && version_ >= 430) {
          const int numFp16Builtins = 4;
          string[numFp16Builtins] atomicFp16Func = [
            " imageAtomicAdd(volatile coherent nontemporal ",
            " imageAtomicMin(volatile coherent nontemporal ",
            " imageAtomicMax(volatile coherent nontemporal ",
            " imageAtomicExchange(volatile coherent nontemporal "
          ];
          const int numFp16DataTypes = 2;
          string[numFp16DataTypes] atomicFp16DataTypes = [
            "f16vec2",
            "f16vec4"
          ];
          for (int j = 0; j < numFp16DataTypes; ++j) {
            for (int i = 0; i < numFp16Builtins; ++i) {
              commonBuiltins.insert(atomicFp16DataTypes[j]);
              commonBuiltins.insert(atomicFp16Func[i]);
              commonBuiltins.insert(imageParams[]);
              commonBuiltins.insert(", ");
              commonBuiltins.insert(atomicFp16DataTypes[j]);
              commonBuiltins.insert(");\n");
            }
          }
        }

        if (profile != EProfile(ES_PROFILE: 1) && version_ >= 450) {
          commonBuiltins.insert("float imageAtomicAdd(volatile coherent nontemporal ");
          commonBuiltins.insert(imageParams[]);
          commonBuiltins.insert(", float);\n");

          commonBuiltins.insert("float imageAtomicAdd(volatile coherent nontemporal ");
          commonBuiltins.insert(imageParams[]);
          commonBuiltins.insert(", float");
          commonBuiltins.insert(", int, int, int);\n");

          commonBuiltins.insert("float imageAtomicExchange(volatile coherent nontemporal ");
          commonBuiltins.insert(imageParams[]);
          commonBuiltins.insert(", float);\n");

          commonBuiltins.insert("float imageAtomicExchange(volatile coherent nontemporal ");
          commonBuiltins.insert(imageParams[]);
          commonBuiltins.insert(", float");
          commonBuiltins.insert(", int, int, int);\n");

          commonBuiltins.insert("float imageAtomicLoad(readonly volatile coherent nontemporal ");
          commonBuiltins.insert(imageParams[]);
          commonBuiltins.insert(", int, int, int);\n");

          commonBuiltins.insert("void imageAtomicStore(writeonly volatile coherent nontemporal ");
          commonBuiltins.insert(imageParams[]);
          commonBuiltins.insert(", float");
          commonBuiltins.insert(", int, int, int);\n");

          commonBuiltins.insert("float imageAtomicMin(volatile coherent nontemporal ");
          commonBuiltins.insert(imageParams[]);
          commonBuiltins.insert(", float);\n");

          commonBuiltins.insert("float imageAtomicMin(volatile coherent nontemporal ");
          commonBuiltins.insert(imageParams[]);
          commonBuiltins.insert(", float");
          commonBuiltins.insert(", int, int, int);\n");

          commonBuiltins.insert("float imageAtomicMax(volatile coherent nontemporal ");
          commonBuiltins.insert(imageParams[]);
          commonBuiltins.insert(", float);\n");

          commonBuiltins.insert("float imageAtomicMax(volatile coherent nontemporal ");
          commonBuiltins.insert(imageParams[]);
          commonBuiltins.insert(", float");
          commonBuiltins.insert(", int, int, int);\n");
        }
      }
    }

    if (sampler.dim == TSamplerDim.EsdRect || sampler.dim == TSamplerDim.EsdBuffer || sampler.shadow || sampler.isMultiSample())
      return;

    if (profile == EProfile(ES_PROFILE: 1) || version_ < 450)
      return;

    DList!string imageLodParams = DList!string([typeName]);
    if (dims == 1)
      imageLodParams.insert(", int");
    else {
      imageLodParams.insert(", ivec");
      imageLodParams.insert(postfixes[dims]);
    }
    imageLodParams.insert(", int");

    commonBuiltins.insert(prefixes[sampler.type]);
    commonBuiltins.insert("vec4 imageLoadLodAMD(readonly volatile coherent nontemporal ");
    commonBuiltins.insert(imageLodParams[]);
    commonBuiltins.insert(");\n");

    commonBuiltins.insert("void imageStoreLodAMD(writeonly volatile coherent nontemporal ");
    commonBuiltins.insert(imageLodParams[]);
    commonBuiltins.insert(", ");
    commonBuiltins.insert(prefixes[sampler.type]);
    commonBuiltins.insert("vec4);\n");

    if (!sampler.is1D()) {
      commonBuiltins.insert("int sparseImageLoadLodAMD(readonly volatile coherent nontemporal ");
      commonBuiltins.insert(imageLodParams[]);
      commonBuiltins.insert(", out ");
      commonBuiltins.insert(prefixes[sampler.type]);
      commonBuiltins.insert("vec4");
      commonBuiltins.insert(");\n");
    }
  }

  void addQueryFunctions(TSampler sampler, string typeName, int version_, EProfile profile) {
    int sizeDims = dimMap[sampler.dim] + (sampler.arrayed ? 1 : 0) - (sampler.dim == TSamplerDim.EsdCube ? 1 : 0);

    if (sampler.isImage() && ((profile == EProfile(ES_PROFILE: 1) && version_ < 310) || (profile != EProfile(ES_PROFILE: 1) && version_ < 420)))
      return;

    if (profile == EProfile(ES_PROFILE: 1))
      commonBuiltins.insert("highp ");
    if (sizeDims == 1)
      commonBuiltins.insert("int");
    else {
      commonBuiltins.insert("ivec");
      commonBuiltins.insert(postfixes[sizeDims]);
    }
    if (sampler.isImage())
      commonBuiltins.insert(" imageSize(readonly writeonly volatile coherent nontemporal ");
    else
      commonBuiltins.insert(" textureSize(");
    commonBuiltins.insert(typeName);
    if (!sampler.isImage() && !sampler.isRect() && !sampler.isBuffer() && !sampler.isMultiSample())
      commonBuiltins.insert(",int);\n");
    else
      commonBuiltins.insert(");\n");
  
    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 430 && sampler.isMultiSample()) {
      commonBuiltins.insert("int ");
      if (sampler.isImage())
        commonBuiltins.insert("imageSamples(readonly writeonly volatile coherent nontemporal ");
      else
        commonBuiltins.insert("textureSamples(");
      commonBuiltins.insert(typeName);
      commonBuiltins.insert(");\n");
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 150 && sampler.isCombined() && sampler.dim != TSamplerDim.EsdRect &&
      !sampler.isMultiSample() && !sampler.isBuffer()) {

      const string[2] funcName = ["vec2 textureQueryLod(", "vec2 textureQueryLOD("];

      for (int i = 0; i < 2; ++i){
        for (int f16TexAddr = 0; f16TexAddr < 2; ++f16TexAddr) {
          if (f16TexAddr && sampler.type != TBasicType.EbtFloat16)
            continue;
          stageBuiltins.STAGE_FRAGMENT.insert(funcName[i]);
          stageBuiltins.STAGE_FRAGMENT.insert(typeName);
          if (dimMap[sampler.dim] == 1)
            if (f16TexAddr)
              stageBuiltins.STAGE_FRAGMENT.insert(", float16_t");
            else
              stageBuiltins.STAGE_FRAGMENT.insert(", float");
          else {
            if (f16TexAddr)
              stageBuiltins.STAGE_FRAGMENT.insert(", f16vec");
            else
              stageBuiltins.STAGE_FRAGMENT.insert(", vec");
            stageBuiltins.STAGE_FRAGMENT.insert(postfixes[dimMap[sampler.dim]]);
          }
          stageBuiltins.STAGE_FRAGMENT.insert(");\n");
        }

        stageBuiltins.STAGE_COMPUTE.insert(funcName[i]);
        stageBuiltins.STAGE_COMPUTE.insert(typeName);
        if (dimMap[sampler.dim] == 1)
          stageBuiltins.STAGE_COMPUTE.insert(", float");
        else {
          stageBuiltins.STAGE_COMPUTE.insert(", vec");
          stageBuiltins.STAGE_COMPUTE.insert(postfixes[dimMap[sampler.dim]]);
        }
        stageBuiltins.STAGE_COMPUTE.insert(");\n");
      }
    }

    if (profile != EProfile(ES_PROFILE: 1) && version_ >= 430 && !sampler.isImage() && sampler.dim != TSamplerDim.EsdRect &&
      !sampler.isMultiSample() && !sampler.isBuffer()) {
      commonBuiltins.insert("int textureQueryLevels(");
      commonBuiltins.insert(typeName);
      commonBuiltins.insert(");\n");
    }
  }

  void addGatherFunctions(TSampler sampler, string typeName, int version_, EProfile profile) {
    switch (sampler.dim) {
      case TSamplerDim.Esd2D:
      case TSamplerDim.EsdRect:
      case TSamplerDim.EsdCube:
        break;
      default:
        return;
    }

    if (sampler.isMultiSample())
      return;

    if (version_ < 140 && sampler.dim == TSamplerDim.EsdRect && sampler.type != TBasicType.EbtFloat)
      return;

    for (int f16TexAddr = 0; f16TexAddr <= 1; ++f16TexAddr) {
      if (f16TexAddr && sampler.type != TBasicType.EbtFloat16)
        continue;
      for (int offset = 0; offset < 3; ++offset) {
        for (int comp = 0; comp < 2; ++comp) {
          if (comp > 0 && sampler.shadow)
            continue;

          if (offset > 0 && sampler.dim == TSamplerDim.EsdCube)
            continue;

          for (int sparse = 0; sparse <= 1; ++sparse) {
            if (sparse && (profile == EProfile(ES_PROFILE: 1) || version_ < 450))
              continue;

            DList!string s;

            if (sparse)
              s.insert("int ");
            else {
              s.insert(prefixes[sampler.type]);
              s.insert("vec4 ");
            }

            if (sparse)
              s.insert("sparseTextureGather");
            else
              s.insert("textureGather");
            switch (offset) {
              case 1:
                s.insert("Offset");
                break;
              case 2:
                s.insert("Offsets");
                break;
              default:
                break;
            }
            if (sparse)
              s.insert("ARB");
            s.insert("(");

            s.insert(typeName);

            if (f16TexAddr)
              s.insert(",f16vec");
            else
              s.insert(",vec");
            int totalDims = dimMap[sampler.dim] + (sampler.arrayed ? 1 : 0);
            s.insert(postfixes[totalDims]);

            if (sampler.shadow)
              s.insert(",float");

            if (offset > 0) {
              s.insert(",ivec2");
              if (offset == 2)
                s.insert("[4]");
            }

            if (sparse) {
              s.insert(",out ");
              s.insert(prefixes[sampler.type]);
              s.insert("vec4 ");
            }

            if (comp)
              s.insert(",int");

            s.insert(");\n");
            commonBuiltins.insert(s[]);
          }
        }
      }
    }

    if (sampler.dim == TSamplerDim.EsdRect || sampler.shadow)
      return;

    if (profile == EProfile(ES_PROFILE: 1) || version_ < 450)
      return;

    for (int bias = 0; bias < 2; ++bias) {
      for (int lod = 0; lod < 2; ++lod) {
        if ((lod && bias) || (lod == 0 && bias == 0))
          continue;

        for (int f16TexAddr = 0; f16TexAddr <= 1; ++f16TexAddr) {
          if (f16TexAddr && sampler.type != TBasicType.EbtFloat16)
            continue;

          for (int offset = 0; offset < 3; ++offset) {
            for (int comp = 0; comp < 2; ++comp) {
              if (comp == 0 && bias)
                continue;

              if (offset > 0 && sampler.dim == TSamplerDim.EsdCube)
                continue;

              for (int sparse = 0; sparse <= 1; ++sparse) {
                if (sparse && (profile == EProfile(ES_PROFILE: 1) || version_ < 450))
                  continue;

                DList!string s;

                if (sparse)
                  s.insert("int ");
                else {
                  s.insert(prefixes[sampler.type]);
                  s.insert("vec4 ");
                }

                if (sparse)
                  s.insert("sparseTextureGather");
                else
                  s.insert("textureGather");

                if (lod)
                  s.insert("Lod");

                switch (offset) {
                  case 1:
                    s.insert("Offset");
                    break;
                  case 2:
                    s.insert("Offsets");
                    break;
                  default:
                    break;
                }

                if (lod)
                  s.insert("AMD");
                else if (sparse)
                  s.insert("ARB");

                s.insert("(");

                s.insert(typeName);

                if (f16TexAddr)
                  s.insert(",f16vec");
                else
                  s.insert(",vec");
                int totalDims = dimMap[sampler.dim] + (sampler.arrayed ? 1 : 0);
                s.insert(postfixes[totalDims]);

                if (lod) {
                  if (f16TexAddr)
                    s.insert(",float16_t");
                  else
                    s.insert(",float");
                }

                if (offset > 0) {
                  s.insert(",ivec2");
                  if (offset == 2)
                    s.insert("[4]");
                }

                if (sparse) {
                  s.insert(",out ");
                  s.insert(prefixes[sampler.type]);
                  s.insert("vec4 ");
                }

                if (comp)
                  s.insert(",int");

                if (bias) {
                  if (f16TexAddr)
                    s.insert(",float16_t");
                  else
                    s.insert(",float");
                }

                s.insert(");\n");
                if (bias)
                  stageBuiltins.STAGE_FRAGMENT.insert(s[]);
                else
                  commonBuiltins.insert(s[]);
              }
            }
          }
        }
      }
    }
  }

  void addSamplingFunctions(TSampler sampler, string typeName, int version_, EProfile profile) {
    for (int proj = 0; proj <= 1; ++proj) {
      if (proj && (sampler.dim == TSamplerDim.EsdCube || sampler.isBuffer() || sampler.arrayed || sampler.isMultiSample()
        || !sampler.isCombined()))
        continue;

      for (int lod = 0; lod <= 1; ++lod) {
        if (lod && (sampler.isBuffer() || sampler.isRect() || sampler.isMultiSample() || !sampler.isCombined()))
          continue;
        if (lod && sampler.dim == TSamplerDim.Esd2D && sampler.arrayed && sampler.shadow)
          continue;
        if (lod && sampler.dim == TSamplerDim.EsdCube && sampler.shadow)
          continue;

        for (int bias = 0; bias <= 1; ++bias) {
          if (bias && (lod || sampler.isMultiSample() || !sampler.isCombined()))
            continue;
          if (bias && (sampler.dim == TSamplerDim.Esd2D || sampler.dim == TSamplerDim.EsdCube) && sampler.shadow && sampler.arrayed)
            continue;
          if (bias && (sampler.isRect() || sampler.isBuffer()))
            continue;

          for (int offset = 0; offset <= 1; ++offset) {
            if (proj + offset + bias + lod > 3)
              continue;
            if (offset && (sampler.dim == TSamplerDim.EsdCube || sampler.isBuffer() || sampler.isMultiSample()))
              continue;

            for (int fetch = 0; fetch <= 1; ++fetch) {
              if (proj + offset + fetch + bias + lod > 3)
                continue;
              if (fetch && (lod || bias))
                continue;
              if (fetch && (sampler.shadow || sampler.dim == TSamplerDim.EsdCube))
                continue;
              if (fetch == 0 && (sampler.isMultiSample() || sampler.isBuffer()
                || !sampler.isCombined()))
                continue;

              for (int grad = 0; grad <= 1; ++grad) {
                if (grad && (lod || bias || sampler.isMultiSample() || !sampler.isCombined()))
                  continue;
                if (grad && sampler.isBuffer())
                  continue;
                if (proj + offset + fetch + grad + bias + lod > 3)
                  continue;

                for (int extraProj = 0; extraProj <= 1; ++extraProj) {
                  bool compare = false;
                  int totalDims = dimMap[sampler.dim] + (sampler.arrayed ? 1 : 0);
                  if (sampler.shadow && totalDims < 2)
                    totalDims = 2;
                  totalDims += (sampler.shadow ? 1 : 0) + proj;
                  if (totalDims > 4 && sampler.shadow) {
                    compare = true;
                    totalDims = 4;
                  }
                  assert(totalDims <= 4);

                  if (extraProj && ! proj)
                    continue;
                  if (extraProj && (sampler.dim == TSamplerDim.Esd3D || sampler.shadow || !sampler.isCombined()))
                    continue;

                  for (int f16TexAddr = 0; f16TexAddr <= 1; ++f16TexAddr) {
                    if (f16TexAddr && sampler.type != TBasicType.EbtFloat16)
                      continue;
                    if (f16TexAddr && sampler.shadow && ! compare) {
                      compare = true;
                      totalDims--;
                    }
                    for (int lodClamp = 0; lodClamp <= 1; ++lodClamp) {
                      if (lodClamp && (profile == EProfile(ES_PROFILE: 1) || version_ < 450))
                        continue;
                      if (lodClamp && (proj || lod || fetch))
                        continue;

                      for (int sparse = 0; sparse <= 1; ++sparse) {
                        if (sparse && (profile == EProfile(ES_PROFILE: 1) || version_ < 450))
                          continue;
                        if (sparse && (sampler.is1D() || sampler.isBuffer() || proj))
                          continue;

                        DList!string s;

                        if (sparse)
                          s.insert("int ");
                        else {
                          if (sampler.shadow)
                            if (sampler.type == TBasicType.EbtFloat16)
                              s.insert("float16_t ");
                            else
                              s.insert("float ");
                          else {
                            s.insert(prefixes[sampler.type]);
                            s.insert("vec4 ");
                          }
                        }

                        if (sparse) {
                          if (fetch)
                            s.insert("sparseTexel");
                          else
                            s.insert("sparseTexture");
                        } else {
                          if (fetch)
                            s.insert("texel");
                          else
                            s.insert("texture");
                        }
                        if (proj)
                          s.insert("Proj");
                        if (lod)
                          s.insert("Lod");
                        if (grad)
                          s.insert("Grad");
                        if (fetch)
                          s.insert("Fetch");
                        if (offset)
                          s.insert("Offset");
                        if (lodClamp)
                          s.insert("Clamp");
                        if (lodClamp != 0 || sparse)
                          s.insert("ARB");
                        s.insert("(");

                        s.insert(typeName);
                        if (extraProj) {
                          if (f16TexAddr)
                            s.insert(",f16vec4");
                          else
                            s.insert(",vec4");
                        } else {
                          s.insert(",");
                          TBasicType t = fetch ? TBasicType.EbtInt : (f16TexAddr ? TBasicType.EbtFloat16 : TBasicType.EbtFloat);
                          if (totalDims == 1)
                            s.insert(getBasicString(t));
                          else {
                            s.insert(prefixes[t]);
                            s.insert("vec");
                            s.insert(postfixes[totalDims]);
                          }
                        }

                        if (compare)
                          s.insert(",float");

                        if ((fetch && !sampler.isBuffer() &&
                          !sampler.isRect() && !sampler.isMultiSample())
                          || (sampler.isMultiSample() && fetch))
                          s.insert(",int");

                        if (lod) {
                          if (f16TexAddr)
                            s.insert(",float16_t");
                          else
                            s.insert(",float");
                        }

                        if (grad) {
                          if (dimMap[sampler.dim] == 1) {
                            if (f16TexAddr)
                              s.insert(",float16_t,float16_t");
                            else
                              s.insert(",float,float");
                          } else {
                            if (f16TexAddr)
                              s.insert(",f16vec");
                            else
                              s.insert(",vec");
                            s.insert(postfixes[dimMap[sampler.dim]]);
                            if (f16TexAddr)
                              s.insert(",f16vec");
                            else
                              s.insert(",vec");
                            s.insert(postfixes[dimMap[sampler.dim]]);
                          }
                        }

                        if (offset) {
                          if (dimMap[sampler.dim] == 1)
                            s.insert(",int");
                          else {
                            s.insert(",ivec");
                            s.insert(postfixes[dimMap[sampler.dim]]);
                          }
                        }

                        if (lodClamp) {
                          if (f16TexAddr)
                            s.insert(",float16_t");
                          else
                            s.insert(",float");
                        }

                        if (sparse) {
                          s.insert(",out ");
                          if (sampler.shadow)
                            if (sampler.type == TBasicType.EbtFloat16)
                              s.insert("float16_t");
                            else
                              s.insert("float");
                          else {
                            s.insert(prefixes[sampler.type]);
                            s.insert("vec4");
                          }
                        }

                        if (bias) {
                          if (f16TexAddr)
                            s.insert(",float16_t");
                          else
                            s.insert(",float");
                        }
                        s.insert(");\n");

                        if (!grad && (bias || lodClamp != 0)) {
                          stageBuiltins.STAGE_FRAGMENT.insert(s[]);
                          stageBuiltins.STAGE_COMPUTE.insert(s[]);
                        } else
                          commonBuiltins.insert(s[]);
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

struct BuiltInFunction {
  TOperator op;
  string name;
  int numArguments;
  ArgType types;
  ArgClass classes;
  Versioning[] versioning;
}

enum ArgType {
  TypeB = 1 << 0,
  TypeF = 1 << 1,
  TypeI = 1 << 2,
  TypeU = 1 << 3,
  TypeF16 = 1 << 4,
  TypeF64 = 1 << 5,
  TypeI8 = 1 << 6,
  TypeI16 = 1 << 7,
  TypeI64 = 1 << 8,
  TypeU8 = 1 << 9,
  TypeU16 = 1 << 10,
  TypeU64 = 1 << 11,
}

enum ArgClass {
  ClassRegular = 0,
  ClassLS = 1 << 0,
  ClassXLS = 1 << 1,
  ClassLS2 = 1 << 2,
  ClassFS = 1 << 3,
  ClassFS2 = 1 << 4,
  ClassLO = 1 << 5,
  ClassB = 1 << 6,
  ClassLB = 1 << 7,
  ClassV1 = 1 << 8,
  ClassFIO = 1 << 9,
  ClassRS = 1 << 10,
  ClassNS = 1 << 11,
  ClassCVN = 1 << 12,
  ClassFO = 1 << 13,
  ClassV3 = 1 << 14,
}

struct Versioning {
  EProfile profiles;
  int minExtendedVersion;
  int minCoreVersion;
  int numExtensions;
  string[] extensions;
}

enum ARBCompatibility = true;

bool IncludeLegacy(int version_, EProfile profile, in SpvVersion spvVersion) {
  return profile != EProfile(ES_PROFILE: 1) && (version_ <= 130 ||
    (spvVersion.spv == 0 && version_ == 140 && ARBCompatibility) ||
    profile == EProfile(COMPATIBILITY_PROFILE: 1));
}

bool ValidVersion(
  in BuiltInFunction func, int version_,
  EProfile profile, in SpvVersion
) {
  if (func.versioning.empty) return true;

  foreach (const ref v; func.versioning) {
    if (v.profiles & profile) {
      if (v.minCoreVersion <= version_ ||
        (v.numExtensions > 0 && v.minExtendedVersion <= version_))
        return true;
    }
  }

  return false;
}

void AddTabledBuiltin(ref DList!string decls, in BuiltInFunction func) {
  static auto isScalarType(int type) {
    return (type & TypeStringColumnMask) == 0;
  }

  const ArgClass ClassFixed = 
    ArgClass.ClassLS | ArgClass.ClassXLS | ArgClass.ClassLS2 |
    ArgClass.ClassFS | ArgClass.ClassFS2;
  for (int fixed = 0; fixed < ((func.classes & ClassFixed) > 0 ? 2 : 1); ++fixed) {
    if (fixed == 0 && (func.classes & ArgClass.ClassXLS))
      continue;

    for (int type = 0; type < TypeStringCount; ++type) {
      if ((func.types & (1 << (type >> TypeStringRowShift))) == 0)
        continue;

      if ((func.classes & ArgClass.ClassV1) && !isScalarType(type))
        continue;

      if ((func.classes & ArgClass.ClassV3) && (type & TypeStringColumnMask) != 2)
        continue;

      if (fixed == 1 && type == (type & TypeStringScalarMask) &&
        (func.classes & ArgClass.ClassXLS) == 0)
        continue;

      if ((func.classes & ArgClass.ClassNS) && isScalarType(type))
        continue;

      if (func.classes & ArgClass.ClassB)
        decls ~= TypeString[type & TypeStringColumnMask];
      else if (func.classes & ArgClass.ClassRS)
        decls ~= TypeString[type & TypeStringScalarMask];
      else
        decls ~= TypeString[type];
      decls ~= " ";
      decls ~= func.name;
      decls ~= "(";

      for (int arg = 0; arg < func.numArguments; ++arg) {
        if (arg == func.numArguments - 1 && (func.classes & ArgClass.ClassLO))
          decls ~= "out ";
        if (arg == 0) {
          if (func.classes & ArgClass.ClassCVN)
              decls ~= "coherent volatile nontemporal ";
          if (func.classes & ArgClass.ClassFIO)
              decls ~= "inout ";
          if (func.classes & ArgClass.ClassFO)
              decls ~= "out ";
        }
        if ((func.classes & ArgClass.ClassLB) && arg == func.numArguments - 1)
          decls ~= TypeString[type & TypeStringColumnMask];
        else if (fixed && (
          (arg == func.numArguments - 1 && (func.classes & (ArgClass.ClassLS | ArgClass.ClassXLS | ArgClass.ClassLS2))) ||
          (arg == func.numArguments - 2 && (func.classes & ArgClass.ClassLS2)) ||
          (arg == 0 && (func.classes & (ArgClass.ClassFS | ArgClass.ClassFS2))) ||
          (arg == 1 && (func.classes & ArgClass.ClassFS2))))
          decls ~= TypeString[type & TypeStringScalarMask];
        else
          decls ~= TypeString[type];
        if (arg < func.numArguments - 1)
          decls ~= ",";
      }
      decls ~= ");\n";
    }
  }
}
