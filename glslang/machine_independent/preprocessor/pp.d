module glslang.machine_independent.preprocessor.pp;
import glslang;

enum EFixedAtoms {
  PpAtomMaxSingle = 127,
  PpAtomBadToken,
  PPAtomAddAssign, PPAtomSubAssign, PPAtomMulAssign, PPAtomDivAssign, PPAtomModAssign,
  PpAtomRight, PpAtomLeft,
  PpAtomRightAssign, PpAtomLeftAssign, PpAtomAndAssign, PpAtomOrAssign, PpAtomXorAssign,
  PpAtomAnd, PpAtomOr, PpAtomXor,
  PpAtomEQ, PpAtomNE, PpAtomGE, PpAtomLE,
  PpAtomDecrement, PpAtomIncrement,
  PpAtomColonColon,
  PpAtomPaste,
  PpAtomConstInt, PpAtomConstUint, PpAtomConstInt64, PpAtomConstUint64, PpAtomConstInt16, PpAtomConstUint16, PpAtomConstFloat, PpAtomConstDouble, PpAtomConstFloat16, PpAtomConstString,
  PpAtomIdentifier,
  PpAtomDefine, PpAtomUndef,
  PpAtomIf, PpAtomIfdef, PpAtomIfndef, PpAtomElse, PpAtomElif, PpAtomEndif,
  PpAtomLine, PpAtomPragma, PpAtomError,
  PpAtomVersion, PpAtomCore, PpAtomCompatibility, PpAtomEs,
  PpAtomExtension,
  PpAtomLineMacro, PpAtomFileMacro, PpAtomVersionMacro,
  PpAtomInclude,
  PpAtomLast,
}

enum eval_prec {
  MIN_PRECEDENCE,
  COND, LOGOR, LOGAND, OR, XOR, AND, EQUAL, RELATION, SHIFT, ADD, MUL, UNARY,
  MAX_PRECEDENCE
}

struct TPpToken {
  import std.container.dlist;

  DList!string name;
  union {
    int ival;
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


struct TokenStream {
  struct Token {
    int atom;
    bool space;
    long i64val;
    string name;

    this(int a, ref TPpToken ppToken) {
      atom = a;
      space = ppToken.space;
      i64val = ppToken.i64val;
      name = ppToken.nameStr;
    }

    int get(ref TPpToken ppToken) {
      ppToken.clear;
      ppToken.space = space;
      ppToken.i64val = i64val;
      ppToken.nameStr = name;
      return atom;
    }
  }

  bool atEnd() => currentPos >= stream.length;

  int getToken(TParseContextBase parseContext, ref TPpToken ppToken) {
    if (atEnd)
      return EndOfInput;

    int atom = stream[currentPos++].get(ppToken);
    ppToken.loc = parseContext.getCurrentLoc;

    return atom;
  }

  void putToken(int atom, ref TPpToken ppToken) {
    stream.insert = Token(atom, ppToken);
  }

  void reset() { currentPos = 0; }

  TChunked!Token stream;
  size_t currentPos;
}

struct MacroSymbol {
  int[] args;
  TokenStream body;
  byte functionLike;
  byte busy;
  byte undef;
}

struct TChunked(T) {
  import std.container.dlist;

  struct Chunk {
    T[ubyte.max] data;
    ubyte length;
  }

  DList!Chunk chunks;

  void insert(T elem) {
    if (chunks.empty || chunks.back.length == ubyte.max)
      chunks.insert = Chunk();
    chunks.back.data[chunks.back.length++] = elem;
  }

  size_t length() {
    size_t len;
    foreach (ref chunk; chunks)
      len += chunk.length;
    return len;
  }

  ref T opIndex(size_t i) { 
    size_t seen;
    foreach (ref chunk; chunks) {
      if (seen + chunk.length > i)
        return chunk.data[i - seen];
      seen += chunk.length;
    }
    throw new Exception("Chunked list lookup failed!");
  }
}
