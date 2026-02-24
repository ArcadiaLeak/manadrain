module glslang.machine_independent.parse_context_base;

import glslang;

import std.format;

struct TPragma {
  this(bool o, bool d) @safe { optimize = o; debug_ = d; }
  bool optimize;
  bool debug_;
  TPragmaTable pragmaTable;
}

class TParseContextBase : TParseVersions {
  string scopeMangler;
  TSymbolTable symbolTable;
  int statementNestingLevel;
  int loopNestingLevel;
  int structNestingLevel;
  int blockNestingLevel;
  int controlFlowNestingLevel;
  TType currentFunctionType;
  bool postEntryPointReturn;
  TPragma contextPragma;
  int beginInvocationInterlockCount;
  int endInvocationInterlockCount;
  
  const bool parsingBuiltins;
  TScanContext scanContext;
  TPpContext ppContext;
  string sourceEntryPointName;

  TVariable globalUniformBlock;
  uint globalUniformBinding;
  uint globalUniformSet;

  uint atomicCounterBlockSet;

  this(
    TSymbolTable symbolTable, TIntermediate interm, bool parsingBuiltins, int version_,
    glslang_profile_t profile, in SpvVersion, EShLanguage language,
    TInfoSink infoSink, bool forwardCompatible, glslang_messages_t messages,
    string entryPoint = null
  ) @safe {
    super(
      interm, version_, profile, spvVersion, language, infoSink,
      forwardCompatible, messages
    );
    scopeMangler = "::"; this.symbolTable = symbolTable; statementNestingLevel = 0;
    loopNestingLevel = 0; structNestingLevel = 0; blockNestingLevel = 0; controlFlowNestingLevel = 0;
    currentFunctionType = null; postEntryPointReturn = false; contextPragma = TPragma(true, false);
    beginInvocationInterlockCount = 0; endInvocationInterlockCount = 0;
    this.parsingBuiltins = parsingBuiltins; scanContext = null; ppContext = null;
    globalUniformBlock = null; globalUniformBinding = TQualifier.layoutBindingEnd;
    globalUniformSet = TQualifier.layoutSetEnd; atomicCounterBlockSet = TQualifier.layoutSetEnd;

    if (spvVersion.spv >= glslang_target_language_version_t.TARGET_SPV_1_3)
      intermediate.setUseStorageBuffer();
    
    if (entryPoint != null)
      sourceEntryPointName = entryPoint;
  }

  void setScanContext(TScanContext c) @safe { scanContext = c; }
  void setPpContext(TPpContext c) @safe { ppContext = c; }

  void outputMessage(Args...)(
    in TSourceLoc loc, string szReason, string szToken,
    string szExtraInfoFormat, TPrefixType prefix, Args args
  ) {
    enum int maxSize = MaxTokenLength + 200;
    char[maxSize] szExtraInfo;

    sformat(szExtraInfo, szExtraInfoFormat, args);

    infoSink.info.prefix = prefix;
    infoSink.info.location(loc, cast(bool) (glslang_messages_t.MSG_DISPLAY_ERROR_COLUMN));
    infoSink.info.append = "'" ~ szToken ~ "' : " ~ szReason ~ " " ~ szExtraInfo ~ "\n";

    if (prefix == TPrefixType.EPrefixError) {
      ++numErrors;
    }
  }

  override void warn(
    in TSourceLoc loc, string szReason,
    string szToken, string szExtraInfo
  ) {
    return outputMessage(
      loc, szReason, szToken, szExtraInfo,
      TPrefixType.EPrefixWarning
    );
  }

  override void error(
    in TSourceLoc loc, string szReason,
    string szToken, string szExtraInfo
  ) {
    if (messages & glslang_messages_t.MSG_ONLY_PREPROCESSOR_BIT)
      return;
    if (messages & glslang_messages_t.MSG_ENHANCED && numErrors > 0)
      return;

    outputMessage(
      loc, szReason, szToken, szExtraInfo,
      TPrefixType.EPrefixError
    );

    if ((messages & glslang_messages_t.MSG_CASCADING_ERRORS_BIT) == 0)
      currentScanner.setEndOfInput;
  }

  void ppError(
    in TSourceLoc loc, string szReason,
    string szToken, string szExtraInfo
  ) {
    outputMessage(
      loc, szReason, szToken, szExtraInfo,
      TPrefixType.EPrefixError
    );

    if ((messages & glslang_messages_t.MSG_CASCADING_ERRORS_BIT) == 0)
      currentScanner.setEndOfInput;
  }

  abstract bool lineContinuationCheck(in TSourceLoc, bool endOfComment);

  TPpContext getPpContext() => ppContext;
  TScanContext getScanContext() => scanContext;
}
