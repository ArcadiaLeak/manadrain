module glslang.machine_independent.preprocessor.pp_scanner;
import glslang;

class tInput {
  bool done;
  TPpContext pp;

  this(TPpContext p) { done = false; pp = p; }

  abstract int getch();
  abstract void ungetch();
  abstract int scan(ref TPpToken ppToken);

  bool peekPasting() => false;
  bool peekContinuedPasting(int) => false;
  bool endOfReplacementList() => false;
  bool isMacroInput() => false;
  bool isStringInput() => false;

  void notifyActivated() {}
  void notifyDeleted() {}
}

class tStringInput : tInput {
  TInputScanner input;

  this(TPpContext pp, TInputScanner i) { super(pp); input = i; }

  override bool isStringInput() => true;

  override int getch() {
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

  override void ungetch() {
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

  override int scan(ref TPpToken ppToken) {
    int ch = 0;
    int ii = 0;
    ulong ival = 0;

    bool floatingPointChar(int ch) {
      return ch == '.' || ch == 'e' || ch == 'E' ||
        ch == 'f' || ch == 'F' ||
        ch == 'h' || ch == 'H';
    }

    enum string[] Int64_Extensions = [
      E_GL_ARB_gpu_shader_int64,
      E_GL_EXT_shader_explicit_arithmetic_types,
      E_GL_NV_gpu_shader5,
      E_GL_EXT_shader_explicit_arithmetic_types_int64
    ];

    enum string[] Int16_Extensions = [
      E_GL_AMD_gpu_shader_int16,
      E_GL_EXT_shader_explicit_arithmetic_types,
      E_GL_EXT_shader_explicit_arithmetic_types_int16
    ];

    ppToken.clear;
    ch = getch;
    while (true) {
      while (ch == ' ' || ch == '\t') {
        ppToken.space = true;
        ch = getch;
      }

      ppToken.loc = pp.parseContext.getCurrentLoc;
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
            ppToken.append = ch;
            ch = getch;
          } while (
            (ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '_'
          );

          if (ppToken.name.empty) continue;
          
          ungetch;
          return EFixedAtoms.PpAtomIdentifier;
        case '0':
          ppToken.append = ch;
          ch = getch;
          if (ch == 'x' || ch == 'X') {
            bool isUnsigned = false;
            bool isInt64 = false;
            bool isInt16 = false;
            ppToken.append = ch;
            ch = getch;
            if (
              (ch >= '0' && ch <= '9') ||
              (ch >= 'A' && ch <= 'F') ||
              (ch >= 'a' && ch <= 'f')
            ) {
              ival = 0;
              do {
                assert(0);
              } while (
                (ch >= '0' && ch <= '9') ||
                (ch >= 'A' && ch <= 'F') ||
                (ch >= 'a' && ch <= 'f')
              );
            } else {
              assert(0);
            }
            assert(0);
          }
      }

      ch = getch;
    }
  }
}

class tMarkerInput : tInput {
  enum int marker = -3;

  this(TPpContext pp) {
    super(pp);
  }
}

class tStringifyLevelInput : tInput {
  enum int PUSH = -4;
  enum int POP = -5;

  this(TPpContext pp) {
    super(pp);
  }
}

class tUngotTokenInput : tInput {
  int token;
  TPpToken lval;
  
  this(TPpContext pp, int t, TPpToken p) {
    super(pp);
    token = t;
    lval = p;
  }

  override int getch() { assert(0, "Unreachable!"); return EndOfInput; }
  override void ungetch() { assert(0, "Unreachable!"); }

  override int scan(ref TPpToken ppToken) {
    if (done)
      return EndOfInput;

    int ret = token;
    ppToken = lval;
    done = true;

    return ret;
  }
}
