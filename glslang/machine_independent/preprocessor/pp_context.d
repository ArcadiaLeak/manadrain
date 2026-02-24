module glslang.machine_independent.preprocessor.pp_context;
import glslang;

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

  int tokenize(TPpToken ppToken) {
    return -1;
  }
}
