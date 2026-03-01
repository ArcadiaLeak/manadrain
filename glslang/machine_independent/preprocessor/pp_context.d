module glslang.machine_independent.preprocessor.pp_context;
import glslang;

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
  import std.container.dlist;
  import std.container.slist;

  MacroSymbol[int] macroDefs;

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

  int ifdepth;
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
        case EFixedAtoms.PpAtomElse:
          if (elseSeen[elsetracker])
            parseContext.ppError(ppToken.loc, "#else after #else", "#else", "");
          elseSeen[elsetracker] = true;
          if (ifdepth == 0)
            parseContext.ppError(ppToken.loc, "mismatched statements", "#else", "");
          token = extraTokenCheck(EFixedAtoms.PpAtomElse, ppToken, scanToken(ppToken));
          token = CPPelse(0, ppToken);
          break;
        default:
          parseContext.ppError(ppToken.loc, "invalid directive:", "#", ppToken.nameStr);
          break;
      }
    } else if (token != '\n' && token != EndOfInput)
      parseContext.ppError(ppToken.loc, "invalid directive", "#", "");

    return token;
  }

  int extraTokenCheck(int contextAtom, ref TPpToken ppToken, int token) {
    if (token != '\n' && token != EndOfInput) {
      enum string message = "unexpected tokens following directive";

      string label;
      if (contextAtom == EFixedAtoms.PpAtomElse)
        label = "#else";
      else if (contextAtom == EFixedAtoms.PpAtomElif)
        label = "#elif";
      else if (contextAtom == EFixedAtoms.PpAtomEndif)
        label = "#endif";
      else if (contextAtom == EFixedAtoms.PpAtomIf)
        label = "#if";
      else if (contextAtom == EFixedAtoms.PpAtomLine)
        label = "#line";
      else
        label = "";

      if (parseContext.relaxedErrors)
        parseContext.ppWarn(ppToken.loc, message, label, "");
      else
        parseContext.ppError(ppToken.loc, message, label, "");

      while (token != '\n' && token != EndOfInput)
        token = scanToken(ppToken);
    }

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
      } else if (pendingPoundSymbols == 1) {
        parseContext.requireProfile(ppToken.loc, ~EProfile(ES_PROFILE: 1), "stringify (#)");
        parseContext.profileRequires(ppToken.loc, ~EProfile(ES_PROFILE: 1), 130, "", "stringify (#)");
        bool isArg = false;
        if (token == EFixedAtoms.PpAtomIdentifier)
          foreach_reverse (arg; mac.args)
            if (stringMap[arg] == ppToken.nameStr) {
              isArg = true;
              break;
            }
        if (!isArg) {
          parseContext.ppError(ppToken.loc, "'#' is not followed by a macro parameter.", "#", "");
          return token;
        }
        mac.body.putToken(tStringifyLevelInput.PUSH, ppToken);
        mac.body.putToken(token, ppToken);
        mac.body.putToken(tStringifyLevelInput.POP, ppToken);
        pendingPoundSymbols = 0;
      } else if (pendingPoundSymbols % 2 == 0) {
        parseContext.requireProfile(ppToken.loc, ~EProfile(ES_PROFILE: 1), "token pasting (##)");
        parseContext.profileRequires(ppToken.loc, ~EProfile(ES_PROFILE: 1), 130, "", "token pasting (##)");
        foreach (i; 0..pendingPoundSymbols / 2)
          mac.body.putToken(EFixedAtoms.PpAtomPaste, savePound);
        mac.body.putToken(token, ppToken);
        pendingPoundSymbols = 0;
      } else {
        parseContext.ppError(ppToken.loc, "Illegal sequence of paste (##) and stringify (#).", "#", "");
        return token;
      }
      token = scanToken(ppToken);
    }
    if (pendingPoundSymbols != 0)
      parseContext.ppError(ppToken.loc, "Macro ended with incomplete '#' paste/stringify operators", "#", "");

    MacroSymbol* existing = defAtom in macroDefs;
    if (existing !is null) {
      if (!existing.undef) {
        if (existing.functionLike != mac.functionLike)
          parseContext.ppError(
            defineLoc, "Macro redefined; function-like versus object-like:",
            "#define", stringMap[defAtom]
          );
        else if (existing.args.length != mac.args.length)
          parseContext.ppError(
            defineLoc, "Macro redefined; different number of arguments:",
            "#define", stringMap[defAtom]
          );
        else {
          if (existing.args != mac.args)
            parseContext.ppError(
              defineLoc, "Macro redefined; different argument names:",
              "#define", stringMap[defAtom]
            );
          existing.body.reset;
          mac.body.reset;
          int newToken;
          bool firstToken = true;
          do {
            int oldToken;
            TPpToken oldPpToken;
            TPpToken newPpToken;
            oldToken = existing.body.getToken(parseContext, oldPpToken);
            newToken = mac.body.getToken(parseContext, newPpToken);
            if (firstToken) {
              newPpToken.space = oldPpToken.space;
              firstToken = false;
            }
            if (oldToken != newToken || oldPpToken != newPpToken) {
              parseContext.ppError(
                defineLoc, "Macro redefined; different substitutions:",
                "#define", stringMap[defAtom]
              );
              break;
            }
          } while (newToken != EndOfInput);
        }
      }
      *existing = mac;
    } else
      addMacroDef(defAtom, mac);

    return '\n';
  }

  void addMacroDef(int atom, MacroSymbol macroDef) {
    macroDefs[atom] = macroDef;
  }

  int eval(
    int token, int precedence, bool shortCircuit,
    ref int res, ref bool err, ref TPpToken ppToken
  ) {
    assert(0);
  }

  int CPPif(ref TPpToken ppToken) {
    int token = scanToken(ppToken);
    if (ifdepth >= maxIfNesting || elsetracker >= maxIfNesting) {
      parseContext.ppError(ppToken.loc, "maximum nesting depth exceeded", "#if", "");
      return EndOfInput;
    } else {
      elsetracker++;
      ifdepth++;
    }
    int res = 0;
    bool err = false;
    token = eval(token, eval_prec.MIN_PRECEDENCE, false, res, err, ppToken);
    token = extraTokenCheck(EFixedAtoms.PpAtomIf, ppToken, token);
    if (!res && !err)
      token = CPPelse(1, ppToken);
    
    return token;
  }

  int CPPelse(int matchelse, ref TPpToken ppToken) {
    inElseSkip = true;
    int depth = 0;
    int token = scanToken(ppToken);

    while (token != EndOfInput) {
      if (token != '#') {
        while (token != '\n' && token != EndOfInput)
          token = scanToken(ppToken);
        
        if (token == EndOfInput)
          return token;

        token = scanToken(ppToken);
        continue;
      }

      if ((token = scanToken(ppToken)) != EFixedAtoms.PpAtomIdentifier)
        continue;

      int nextAtom = atomMap[ppToken.nameStr];
      if (
        nextAtom == EFixedAtoms.PpAtomIf ||
        nextAtom == EFixedAtoms.PpAtomIfdef ||
        nextAtom == EFixedAtoms.PpAtomIfndef
      ) {
        depth++;
        if (ifdepth >= maxIfNesting || elsetracker >= maxIfNesting) {
          parseContext.ppError(ppToken.loc, "maximum nesting depth exceeded", "#if/#ifdef/#ifndef", "");
          return EndOfInput;
        } else {
          ifdepth++;
          elsetracker++;
        }
      } else if (nextAtom == EFixedAtoms.PpAtomEndif) {
        token = extraTokenCheck(nextAtom, ppToken, scanToken(ppToken));
        elseSeen[elsetracker] = false;
        --elsetracker;
        if (depth == 0) {
          if (ifdepth > 0)
            --ifdepth;
          break;
        }
        --depth;
        --ifdepth;
      } else if (matchelse && depth == 0) {
        if (nextAtom == EFixedAtoms.PpAtomElse) {
          elseSeen[elsetracker] = true;
          token = extraTokenCheck(nextAtom, ppToken, scanToken(ppToken));
          break;
        } else if (nextAtom == EFixedAtoms.PpAtomElif) {
          if (elseSeen[elsetracker])
            parseContext.ppError(ppToken.loc, "#elif after #else", "#elif", "");
          if (ifdepth > 0) {
            --ifdepth;
            elseSeen[elsetracker] = false;
            --elsetracker;
          }
          inElseSkip = false;
          return CPPif(ppToken);
        } else if (nextAtom == EFixedAtoms.PpAtomElse) {
          if (elseSeen[elsetracker])
            parseContext.ppError(ppToken.loc, "#else after #else", "#else", "");
          else
            elseSeen[elsetracker] = true;
          token = extraTokenCheck(nextAtom, ppToken, scanToken(ppToken));
        } else if (nextAtom == EFixedAtoms.PpAtomElif)
          if (elseSeen[elsetracker])
            parseContext.ppError(ppToken.loc, "#elif after #else", "#elif", "");
      }
    }

    inElseSkip = false;
    return token;
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
