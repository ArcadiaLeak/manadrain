module glslang.machine_independent.preprocessor;
import glslang;

public import glslang.machine_independent.preprocessor.pp_context;
public import glslang.machine_independent.preprocessor.pp_scanner;

enum EFixedAtoms {
  PpAtomMaxSingle = 127,

  PpAtomBadToken,

  PPAtomAddAssign,
  PPAtomSubAssign,
  PPAtomMulAssign,
  PPAtomDivAssign,
  PPAtomModAssign,

  PpAtomRight,
  PpAtomLeft,

  PpAtomRightAssign,
  PpAtomLeftAssign,
  PpAtomAndAssign,
  PpAtomOrAssign,
  PpAtomXorAssign,

  PpAtomAnd,
  PpAtomOr,
  PpAtomXor,

  PpAtomEQ,
  PpAtomNE,
  PpAtomGE,
  PpAtomLE,

  PpAtomDecrement,
  PpAtomIncrement,

  PpAtomColonColon,

  PpAtomPaste,

  PpAtomConstInt,
  PpAtomConstUint,
  PpAtomConstInt64,
  PpAtomConstUint64,
  PpAtomConstInt16,
  PpAtomConstUint16,
  PpAtomConstFloat,
  PpAtomConstDouble,
  PpAtomConstFloat16,
  PpAtomConstString,

  PpAtomIdentifier,

  PpAtomDefine,
  PpAtomUndef,

  PpAtomIf,
  PpAtomIfdef,
  PpAtomIfndef,
  PpAtomElse,
  PpAtomElif,
  PpAtomEndif,

  PpAtomLine,
  PpAtomPragma,
  PpAtomError,

  PpAtomVersion,
  PpAtomCore,
  PpAtomCompatibility,
  PpAtomEs,

  PpAtomExtension,

  PpAtomLineMacro,
  PpAtomFileMacro,
  PpAtomVersionMacro,

  PpAtomInclude,

  PpAtomLast,
}

struct TPpToken {
  import std.container.dlist;

  DList!string name;
  union {
    int val;
    double dval;
    long i64val;
  }
  bool space;
  bool fullyExpanded;
  TSourceLoc loc;

  void clear() {
    space = false;
    i64val = 0;
    loc.clear;
    name.clear;
    fullyExpanded = false;
  }

  string nameStr() {
    import std.array;
    import std.range.primitives;

    if (name[].walkLength > 1)
      nameStr = name[].join;

    return name.front;
  }

  void nameStr(string str) {
    name.clear;
    name.insert = str;
  }

  void append(dchar ch) {
    import std.conv;
    name.insert = ch.to!string;
  }
}
