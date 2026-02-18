module glslang.machine_independent.scan_context;

import glslang;

class TScanContext {
  protected {
    TParseContextBase parseContext;
    bool afterType;
    bool afterStruct;
    bool field;
    bool afterBuffer;
    bool inDeclaratorList;
    bool afterDeclarator;
    int angleBracketDepth;
    int squareBracketDepth;
    int parenDepth;
  }

  this(TParseContextBase pc) @safe {
    parseContext = pc; afterType = false; afterStruct = false; field = false;
    afterBuffer = false; inDeclaratorList = false; afterDeclarator = false;
    angleBracketDepth = 0; squareBracketDepth = 0; parenDepth = 0;
  }

  int tokenize(TPpContext pp, TParserToken token) {
    return -1;
  }
}
