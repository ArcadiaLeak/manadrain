module glslang.machine_independent.preprocessor.pp_context;
import glslang;

import std.container.dlist;
import std.container.slist;

class TPpContext {
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
    for (elsetracker = 0; elsetracker < maxIfNesting; elsetracker++)
      elseSeen[elsetracker] = false;
    elsetracker = 0;
  }

  int tokenize(ref TPpToken ppToken) {
    int stringifyDepth = 0;
    TPpToken stringifiedToken;
    while (true) {
      int token = scanToken(ppToken);

      import std.stdio;
      writeln(token);
    }

    return -1;
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
            break;
          default:
            assert(0);
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
