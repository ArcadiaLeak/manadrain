module glslang.machine_independent.preprocessor.pp_scanner;
import glslang;

class tInput {
  bool done;
  TPpContext pp;

  this(TPpContext p) { done = false; pp = p; }

  abstract int scan(ref TPpToken ppToken);

  void notifyDeleted() {}
}

class tStringInput : tInput {
  TInputScanner input;

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

  override int scan(ref TPpToken ppToken) {
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

    ppToken.clear;
    ch = getch;
    while (true) {
      while (ch == ' ' || ch == '\t') {
        ppToken.space = true;
        ch = getch;
      }

      ppToken.loc = pp.parseContext.getCurrentLoc;
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
              ch = getch;
            } else {
              if (!AlreadyComplained) {
                pp.parseContext.ppError(ppToken.loc, "name too long", "", "");
                AlreadyComplained = 1;
              }
              ch = getch;
            }
          } while (
            (ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '_'
          );

          if (len == 0) continue;

          ppToken.name[len] = '\0';
          ungetch;
          return EFixedAtoms.PpAtomIdentifier;
        case '0':
          ppToken.name[len++] = cast(char) ch;
          ch = getch;
          if (ch == 'x' || ch == 'X') {
            bool isUnsigned = false;
            bool isInt64 = false;
            bool isInt16 = false;
            ppToken.name[len++] = cast(char) ch;
            ch = getch;
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

      ch = getch;
    }
  }
}
