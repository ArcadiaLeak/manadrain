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

  override int getch() { assert(0, "Unreachable!"); return EndOfInput; }
  override void ungetch() { assert(0, "Unreachable!"); }

  override int scan(ref TPpToken ppToken) {
    if (done)
      return EndOfInput;
    done = true;

    return marker;
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

class tZeroInput : tInput {
  this(TPpContext pp) {
    super(pp);
  }

  override int getch() { assert(0, "Unreachable!"); return EndOfInput; }
  override void ungetch() { assert(0, "Unreachable!"); }

  override int scan(ref TPpToken ppToken) {
    if (done)
      return EndOfInput;

    ppToken.nameStr = "0";
    ppToken.ival = 0;
    ppToken.space = false;
    done = true;

    return EFixedAtoms.PpAtomConstInt;
  }
}

class tMacroInput : tInput {
  bool prepaste;
  bool postpaste;

  MacroSymbol* mac;
  TokenStream*[] args;
  TokenStream*[] expandedArgs;
  
  this(TPpContext pp) {
    super(pp);
    prepaste = false;
    postpaste = false;
  }

  override int getch() { assert(0, "Unreachable!"); return EndOfInput; }
  override void ungetch() { assert(0, "Unreachable!"); }

  override int scan(ref TPpToken ppToken) {
    int token;
    do {
      token = mac.body.getToken(pp.parseContext, ppToken);
    } while (token == ' ');

    bool pasting = false;
    if (postpaste) {
      pasting = true;
      postpaste = false;
    }

    if (prepaste) {
      assert(token == EFixedAtoms.PpAtomPaste);
      prepaste = false;
      postpaste = true;
    }

    if (mac.body.peekTokenizedPasting(false)) {
      prepaste = true;
      pasting = true;
    }

    if (token == EFixedAtoms.PpAtomIdentifier) {
      long brokeAt = -1;
      foreach_reverse (i, arg; mac.args)
        if (pp.stringMap[arg] == ppToken.nameStr) {
          brokeAt = i;
          break;
        }
      if (brokeAt >= 0) {
        TokenStream* arg = expandedArgs[brokeAt];
        bool expanded = !!arg && !pasting;
        if (arg is null || pasting)
          arg = args[brokeAt];
        pp.pushTokenStreamInput(arg, prepaste, expanded);
        return pp.scanToken(ppToken);
      }
    }

    if (token == EndOfInput)
      mac.busy = 0;

    return token;
  }
}

class tTokenInput : tInput {
  TokenStream* tokens;
  bool lastTokenPastes;
  bool preExpanded;

  this(TPpContext pp, TokenStream* t, bool prepasting, bool expanded) {
    super(pp);
    tokens = t;
    lastTokenPastes = prepasting;
    preExpanded = expanded;
  }

  override int getch() { assert(0, "Unreachable!"); return EndOfInput; }
  override void ungetch() { assert(0, "Unreachable!"); }

  override int scan(ref TPpToken ppToken) {
    int token = tokens.getToken(pp.parseContext, ppToken);
    ppToken.fullyExpanded = preExpanded;
    if (tokens.atEnd && token == EFixedAtoms.PpAtomIdentifier) {
      int macroAtom = get(pp.atomMap, ppToken.nameStr, 0);
      MacroSymbol* macro_ = macroAtom == 0 ? null : macroAtom in pp.macroDefs;
      if (macro_ && macro_.functionLike)
        ppToken.fullyExpanded = false;
    }
    return token;
  }
}
