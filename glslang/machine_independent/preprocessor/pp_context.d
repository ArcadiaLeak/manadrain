module glslang.machine_independent.preprocessor.pp_context;
import glslang;

import std.container.dlist;
import std.container.slist;
import std.typecons;

enum Tuple!(EFixedAtoms, string)[] tokens = [
  tuple(EFixedAtoms.PPAtomAddAssign, "+="),
  tuple(EFixedAtoms.PPAtomSubAssign, "-="),
  tuple(EFixedAtoms.PPAtomMulAssign, "*="),
  tuple(EFixedAtoms.PPAtomDivAssign, "/="),
  tuple(EFixedAtoms.PPAtomModAssign, "%="),

  tuple(EFixedAtoms.PpAtomRight, ">>"),
  tuple(EFixedAtoms.PpAtomLeft, "<<"),
  tuple(EFixedAtoms.PpAtomAnd, "&&"),
  tuple(EFixedAtoms.PpAtomOr, "||"),
  tuple(EFixedAtoms.PpAtomXor, "^^"),

  tuple(EFixedAtoms.PpAtomRightAssign, ">>="),
  tuple(EFixedAtoms.PpAtomLeftAssign, "<<="),
  tuple(EFixedAtoms.PpAtomAndAssign, "&="),
  tuple(EFixedAtoms.PpAtomOrAssign, "|="),
  tuple(EFixedAtoms.PpAtomXorAssign, "^="),

  tuple(EFixedAtoms.PpAtomEQ, "=="),
  tuple(EFixedAtoms.PpAtomNE, "!="),
  tuple(EFixedAtoms.PpAtomGE, ">="),
  tuple(EFixedAtoms.PpAtomLE, "<="),

  tuple(EFixedAtoms.PpAtomDecrement, "--"),
  tuple(EFixedAtoms.PpAtomIncrement, "++"),

  tuple(EFixedAtoms.PpAtomColonColon, "::"),

  tuple(EFixedAtoms.PpAtomDefine, "define"),
  tuple(EFixedAtoms.PpAtomUndef, "undef"),
  tuple(EFixedAtoms.PpAtomIf, "if"),
  tuple(EFixedAtoms.PpAtomElif, "elif"),
  tuple(EFixedAtoms.PpAtomElse, "else"),
  tuple(EFixedAtoms.PpAtomEndif, "endif"),
  tuple(EFixedAtoms.PpAtomIfdef, "ifdef"),
  tuple(EFixedAtoms.PpAtomIfndef, "ifndef"),
  tuple(EFixedAtoms.PpAtomLine, "line"),
  tuple(EFixedAtoms.PpAtomPragma, "pragma"),
  tuple(EFixedAtoms.PpAtomError, "error"),

  tuple(EFixedAtoms.PpAtomVersion, "version"),
  tuple(EFixedAtoms.PpAtomCore, "core"),
  tuple(EFixedAtoms.PpAtomCompatibility, "compatibility"),
  tuple(EFixedAtoms.PpAtomEs, "es"),
  tuple(EFixedAtoms.PpAtomExtension, "extension"),

  tuple(EFixedAtoms.PpAtomLineMacro, "__LINE__"),
  tuple(EFixedAtoms.PpAtomFileMacro, "__FILE__"),
  tuple(EFixedAtoms.PpAtomVersionMacro, "__VERSION__"),

  tuple(EFixedAtoms.PpAtomInclude, "include"),
];

class TPpContext {
  int[string] atomMap;
  string[int] stringMap;
  int lastAtom;

  string preamble;
  string[] strings;
  int currentString;

  int previous_token;
  TParseContextBase parseContext;
  DList!int lastLineTokens;
  DList!TSourceLoc lastLineTokenLocs;

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

  SList!tInput inputStack;
  bool errorOnVersion;
  bool versionSeen;
  
  this(
    TParseContextBase pc, string rootFileName, TShader.Includer inclr
  ) {
    preamble = null; strings = null; previous_token = '\n'; parseContext = pc;
    includer = inclr; inComment = false; this.rootFileName = rootFileName;
    currentSourceFile = rootFileName; disableEscapeSequences = false;
    inElseSkip = false;

    ifdepth = 0;
    elsetracker = 0;

    enum string s = "~!%^&*()-+=|,.<>/?;:[]{}#\\";
    static foreach (ch; s)
      addAtomFixed(ch, [ch]);
    static foreach (token; tokens)
      addAtomFixed(token.expand);
    lastAtom = EFixedAtoms.PpAtomLast;
  }

  void addAtomFixed(int atom, string s) {
    atomMap[s] = atom;
    stringMap[atom] = s;
  }

  int getAddAtom(string s) {
    int atom = get(atomMap, s, 0);
    if (atom == 0) {
      atom = lastAtom++;
      addAtomFixed(atom, s);
    }
    return atom;
  }

  int tokenize(ref TPpToken ppToken) {
    int stringifyDepth = 0;
    TPpToken stringifiedToken;
    while (true) {
      int token = scanToken(ppToken);
      token = tokenPaste(token, ppToken);

      if (token == EndOfInput) {
        missingEndifCheck;
        return EndOfInput;
      }
      if (token == '#') {
        if (previous_token == '\n') {
          token = readCPPline(ppToken);
          if (token == EndOfInput) {
              missingEndifCheck;
              return EndOfInput;
          }
          continue;
        } else {
          parseContext.ppError(ppToken.loc, "preprocessor directive cannot be preceded by another token", "#", "");
          return EndOfInput;
        }
      }

      import std.stdio;
      writeln(token);

      break;
    }

    return EndOfInput;
  }

  int readCPPline(ref TPpToken ppToken) {
    int token = scanToken(ppToken);
    
    if (token == EFixedAtoms.PpAtomIdentifier) {
      switch (get(atomMap, ppToken.nameStr, 0)) {
        case EFixedAtoms.PpAtomDefine:
          token = CPPdefine(ppToken);
          break;
        default:
          parseContext.ppError(ppToken.loc, "invalid directive:", "#", ppToken.nameStr);
          break;
      }
    } else if (token != '\n' && token != EndOfInput)
      parseContext.ppError(ppToken.loc, "invalid directive", "#", "");

    return token;
  }

  void missingEndifCheck() {
    if (ifdepth > 0)
      parseContext.ppError(parseContext.getCurrentLoc, "missing #endif", "", "");
  }

  int CPPdefine(ref TPpToken ppToken) {
    MacroSymbol mac;

    int token = scanToken(ppToken);
    if (token != EFixedAtoms.PpAtomIdentifier) {
      parseContext.ppError(ppToken.loc, "must be followed by macro name", "#define", "");
      return token;
    }
    if (ppToken.loc.string_ >= 0)
      parseContext.reservedPpErrorCheck(ppToken.loc, ppToken.nameStr, "#define");

    const int defAtom = getAddAtom(ppToken.nameStr);
    TSourceLoc defineLoc = ppToken.loc;

    token = scanToken(ppToken);
    if (token == '(' && !ppToken.space) {
      mac.functionLike = 1;
      do {
        token = scanToken(ppToken);
        if (mac.args.length == 0 && token == ')')
          break;
        if (token != EFixedAtoms.PpAtomIdentifier) {
          parseContext.ppError(ppToken.loc, "bad argument", "#define", "");
          return token;
        }
        const int argAtom = getAddAtom(ppToken.nameStr);

        bool duplicate = false;
        foreach (arg; mac.args) {
          if (arg == argAtom) {
            parseContext.ppError(ppToken.loc, "duplicate macro parameter", "#define", "");
            duplicate = true;
            break;
          }
        }
        if (!duplicate)
          mac.args ~= argAtom;
        token = scanToken(ppToken);
      } while (token == ',');

      if (token != ')') {
        parseContext.ppError(ppToken.loc, "missing parenthesis", "#define", "");
        return token;
      }

      token = scanToken(ppToken);
    } else if (token != '\n' && token != EndOfInput && !ppToken.space) {
      parseContext.ppWarn(ppToken.loc, "missing space after macro name", "#define", "");
      return token;
    }

    int pendingPoundSymbols = 0;
    TPpToken savePound;
    while (token != '\n' && token != EndOfInput) {
      if (token == '#') {
        pendingPoundSymbols++;
        if (pendingPoundSymbols == 0) {
          savePound = ppToken;
        }
      } else if (pendingPoundSymbols == 0) {
        mac.body.putToken(token, ppToken);
      }
    }

    return '\n';
  }

  int scanToken(ref TPpToken ppToken) {
    int token = EndOfInput;

    while (!inputStack.empty) {
      token = inputStack.front.scan(ppToken);
      if (token != EndOfInput || inputStack.empty)
        break;
      popInput;
    }
    if (!inputStack.empty && inputStack.front.isStringInput && !inElseSkip) {
      if (token == '\n') {
        lastLineTokens.clear;
        lastLineTokenLocs.clear;
      } else {
        lastLineTokens ~= token;
        lastLineTokenLocs ~= ppToken.loc;
      }
    }
    return token;
  }

  bool peekPasting() => !inputStack.empty && inputStack.front.peekPasting;
  bool peekContinuedPasting(int a) => !inputStack.empty && inputStack.front.peekContinuedPasting(a);
  bool endOfReplacementList() => inputStack.empty || inputStack.front.endOfReplacementList;

  int tokenPaste(int token, ref TPpToken ppToken) {
    if (token == EFixedAtoms.PpAtomPaste) {
      parseContext.ppError(ppToken.loc, "unexpected location", "##", "");
      return scanToken(ppToken);
    }

    int resultToken = token;

    while (peekPasting) {
      TPpToken pastedPpToken;

      token = scanToken(pastedPpToken);
      assert(token == EFixedAtoms.PpAtomPaste);

      if (endOfReplacementList) {
        parseContext.ppError(ppToken.loc, "unexpected location; end of replacement list", "##", "");
        break;
      }

      do {
        token = scanToken(pastedPpToken);

        if (token == tMarkerInput.marker) {
          parseContext.ppError(ppToken.loc, "unexpected location; end of argument", "##", "");
          return resultToken;
        }

        switch (resultToken) {
          case EFixedAtoms.PpAtomIdentifier:
            break;
          case '=': case '!': case '-': case '~': case '+': case '*':
          case '/': case '%': case '<': case '>': case '|': case '^':
          case '&':
          case EFixedAtoms.PpAtomRight: case EFixedAtoms.PpAtomLeft:
          case EFixedAtoms.PpAtomAnd: case EFixedAtoms.PpAtomOr:
          case EFixedAtoms.PpAtomXor:
            ppToken.nameStr = stringMap[resultToken];
            pastedPpToken.nameStr = stringMap[token];
            break;
          default:
            parseContext.ppError(ppToken.loc, "not supported for these tokens", "##", "");
            return resultToken;
        }

        ppToken.name ~= pastedPpToken.name[];
        if (resultToken != EFixedAtoms.PpAtomIdentifier) {
          uint newToken = get(atomMap, ppToken.nameStr, 0);
          if (newToken > 0)
            resultToken = newToken;
          else
            parseContext.ppError(ppToken.loc, "combined token is invalid", "##", "");
        }
      } while (peekContinuedPasting(resultToken));
    }

    return resultToken;
  }

  void pushInput(tInput input) {
    inputStack.insertFront(input);

    input.notifyActivated;
  }

  void popInput() {
    inputStack.front.notifyDeleted;
  
    inputStack.removeFront;
  }

  void setInput(TInputScanner input, bool versionWillBeError) {
    assert(inputStack.empty);

    pushInput(new tStringInput(this, input));

    errorOnVersion = versionWillBeError;
    versionSeen = false;
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
  }

  void putToken(int atom, ref TPpToken ppToken) {
    stream.insert = Token(atom, ppToken);
  }

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
}