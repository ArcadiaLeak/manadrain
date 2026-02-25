module glslang.machine_independent.preprocessor.pp_context;
import glslang;

import std.container.slist;

class TPpContext {
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

    return token;
  }

  void pushInput(tInput in_) {
    inputStack.insert = in_;
    in_.notifyActivated;
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
