module glslang.machine_independent.parse_helper;
import glslang;

class TParseContext : TParseContextBase {
  this(
    TSymbolTable symbolTable, TIntermediate interm, bool parsingBuiltIns,
    int version_, EProfile profile, in SpvVersion spvVersion,
    EShLanguage language, TInfoSink, bool forwardCompatible,
    EShMessages messages = EShMessages(),
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
      profileRequires(loc, EProfile(ES_PROFILE: 1), 300, "", message);
      profileRequires(loc, ~EProfile(ES_PROFILE: 1), 420, E_GL_ARB_shading_language_420pack, message);
    }

    return lineContinuationAllowed;
  }

  override bool lineDirectiveShouldSetNextLine() const =>
    isEsProfile || version_ >= 330;

  override void reservedPpErrorCheck(in TSourceLoc loc, string identifier, string op) {
    import std.algorithm.searching;

    if (identifier.startsWith("GL_") && !extensionTurnedOn(E_GL_EXT_spirv_intrinsics))
      ppError(loc, "names beginning with \"GL_\" can't be (un)defined:", op,  identifier);
    else if (identifier.startsWith("defined")) {
      if (relaxedErrors)
        ppWarn(loc, "\"defined\" is (un)defined:", op,  identifier);
      else
        ppError(loc, "\"defined\" can't be (un)defined:", op,  identifier);
    }
    else if (identifier.canFind("__") && !extensionTurnedOn(E_GL_EXT_spirv_intrinsics)) {
      if (
        isEsProfile && version_ >= 300 && (
          identifier == "__LINE__" ||
          identifier == "__FILE__" ||
          identifier == "__VERSION__"
        )
      )
        ppError(loc, "predefined names can't be (un)defined:", op,  identifier);
      else {
        if (isEsProfile && version_ < 300 && !relaxedErrors)
          ppError(loc, "names containing consecutive underscores are reserved, and an error if version < 300:", op, identifier);
        else
          ppWarn(loc, "names containing consecutive underscores are reserved:", op, identifier);
      }
    }
  }
}
