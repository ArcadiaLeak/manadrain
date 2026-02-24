module glslang.machine_independent.parse_helper;

import glslang;

class TParseContext : TParseContextBase {
  this(
    TSymbolTable symbolTable, TIntermediate interm, bool parsingBuiltIns,
    int version_, profile_t profile, in SpvVersion spvVersion,
    EShLanguage language, TInfoSink, bool forwardCompatible,
    messages_t messages = messages_t.MSG_DEFAULT_BIT,
    string entryPoint = null
  ) {
    super(
      symbolTable, interm, parsingBuiltIns, version_, profile, spvVersion,
      language, infoSink, forwardCompatible, messages, entryPoint
    );
  }

  override bool lineContinuationCheck(in TSourceLoc loc, bool endOfComment) {
    string message = "line continuation";

    bool lineContinuationAllowed = (isEsProfile && version_ >= 300) ||
      (!isEsProfile && (version_ >= 420 || extensionTurnedOn(E_GL_ARB_shading_language_420pack)));
  
    if (endOfComment) {
      if (lineContinuationAllowed)
        warn(loc, "used at end of comment; the following line is still part of the comment", message, "");
      else
        warn(loc, "used at end of comment, but this version does not provide line continuation", message, "");

      return lineContinuationAllowed;
    }

    if (relaxedErrors) {
      if (!lineContinuationAllowed)
        warn(loc, "not allowed in this version", message, "");
      return true;
    } else {
      profileRequires(loc, profile_t.ES_PROFILE, 300, "", message);
      profileRequires(loc, ~profile_t.ES_PROFILE, 420, E_GL_ARB_shading_language_420pack, message);
    }

    return lineContinuationAllowed;
  }
}
