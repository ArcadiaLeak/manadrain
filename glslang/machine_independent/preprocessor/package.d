module glslang.machine_independent.preprocessor;
import glslang;

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

class TPpContext {
  protected {
    string preamble;
    string[] strings;
    int currentString;

    int previous_token;
    TParseContextBase parseContext;

    enum int maxIfNesting = 65;

    bool ifdepth;
    bool[maxIfNesting] elseSeen;
    int elsetracker;

    TShader.Includer includer;

    bool inComment;
    string rootFileName;
    string currentSourceFile;

    bool disableEscapeSequences;
    bool inElseSkip;
  }
  
  this(
    TParseContextBase pc, string rootFileName, TShader.Includer inclr
  ) @safe {
    preamble = null; strings = null; previous_token = '\n'; parseContext = pc;
    includer = inclr; inComment = false; this.rootFileName = rootFileName;
    currentSourceFile = rootFileName; disableEscapeSequences = false;
    inElseSkip = false;

    ifdepth = 0;
    for (elsetracker = 0; elsetracker < maxIfNesting; elsetracker++)
      elseSeen[elsetracker] = false;
    elsetracker = 0;
  }
}

class TPpToken {
  char[MaxTokenLength + 1] name;
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
    loc.init();
    name[0] = 0;
    fullyExpanded = false;
  }
}

class tInput {
  protected {
    bool done;
    TPpContext pp;
  }

  this(TPpContext p) { done = false; pp = p; }
}

class tStringInput : tInput {
  protected TInputScanner input;

  this(TPpContext pp, TInputScanner i) { super(pp); input = i; }

  int getch() {
    int ch = input.get;

    if (ch == '\\') {
      do {
        if (input.peek == '\r' || input.peek == '\n') {
          bool allowed = pp.parseContext.lineContinuationCheck(
            input.getSourceLoc, pp.inComment);
          if (!allowed && pp.inComment)
            return '\\';
          
          ch = input.get;
          int nextch = input.get;
          if (ch == '\r' && nextch == '\n')
            ch = input.get;
          else
            ch = nextch;
        } else
          return '\\';
      } while (ch == '\\');
    }

    if (ch == '\r' || ch == '\n') {
      if (ch == '\r' && input.peek == '\n')
        return input.get;
      return '\n';
    }

    return ch;
  }

  void ungetch() {
    input.unget;

    do {
      int ch = input.peek;
      if (ch == '\r' || ch == '\n') {
        if (ch == '\n') {
          input.unget;
          if (input.peek != '\r')
            input.get;
        }
        input.unget;
        if (input.peek == '\\')
          input.unget;
        else {
          input.get;
          break;
        }
      } else
        break;
    } while (true);
  }
}

int scan(tStringInput self, TPpToken ppToken) {
  int AlreadyComplained = 0;
  int len = 0;
  int ch = 0;
  int ii = 0;
  ulong ival = 0;

  bool floatingPointChar(int ch) {
    return ch == '.' || ch == 'e' || ch == 'E' ||
      ch == 'f' || ch == 'F' ||
      ch == 'h' || ch == 'H';
  }

  static immutable string[] Int64_Extensions = [
    E_GL_ARB_gpu_shader_int64,
    E_GL_EXT_shader_explicit_arithmetic_types,
    E_GL_NV_gpu_shader5,
    E_GL_EXT_shader_explicit_arithmetic_types_int64
  ];

  static immutable string[] Int16_Extensions = [
    E_GL_AMD_gpu_shader_int16,
    E_GL_EXT_shader_explicit_arithmetic_types,
    E_GL_EXT_shader_explicit_arithmetic_types_int16
  ];

  ppToken.clear();
  ch = self.getch();
  while (true) {
    while (ch == ' ' || ch == '\t') {
      ppToken.space = true;
      ch = self.getch();
    }

    ppToken.loc = self.pp.parseContext.getCurrentLoc;
    len = 0;
    switch (ch) {
      default:
        if (ch > EFixedAtoms.PpAtomMaxSingle)
          ch = EFixedAtoms.PpAtomBadToken;
        return ch;

      case 'A': case 'B': case 'C': case 'D': case 'E':
      case 'F': case 'G': case 'H': case 'I': case 'J':
      case 'K': case 'L': case 'M': case 'N': case 'O':
      case 'P': case 'Q': case 'R': case 'S': case 'T':
      case 'U': case 'V': case 'W': case 'X': case 'Y':
      case 'Z': case '_':
      case 'a': case 'b': case 'c': case 'd': case 'e':
      case 'f': case 'g': case 'h': case 'i': case 'j':
      case 'k': case 'l': case 'm': case 'n': case 'o':
      case 'p': case 'q': case 'r': case 's': case 't':
      case 'u': case 'v': case 'w': case 'x': case 'y':
      case 'z':
        do {
          if (len < MaxTokenLength) {
            ppToken.name[len++] = cast(char) ch;
            ch = self.getch();
          } else {
            if (!AlreadyComplained) {
              self.pp.parseContext.ppError(ppToken.loc, "name too long", "", "");
              AlreadyComplained = 1;
            }
            ch = self.getch();
          }
        } while (
          (ch >= 'a' && ch <= 'z') ||
          (ch >= 'A' && ch <= 'Z') ||
          (ch >= '0' && ch <= '9') ||
          ch == '_'
        );

        if (len == 0) continue;

        ppToken.name[len] = '\0';
        self.ungetch();
        return EFixedAtoms.PpAtomIdentifier;
      case '0':
        ppToken.name[len++] = cast(char) ch;
        ch = self.getch();
        if (ch == 'x' || ch == 'X') {
          bool isUnsigned = false;
          bool isInt64 = false;
          bool isInt16 = false;
          ppToken.name[len++] = cast(char) ch;
          ch = self.getch();
          if (
            (ch >= '0' && ch <= '9') ||
            (ch >= 'A' && ch <= 'F') ||
            (ch >= 'a' && ch <= 'f')
          ) {
            ival = 0;
            do {
              //
            } while (
              (ch >= '0' && ch <= '9') ||
              (ch >= 'A' && ch <= 'F') ||
              (ch >= 'a' && ch <= 'f')
            );
          } else {
            //
          }
          //
        }
    }

    ch = self.getch();
  }
}
