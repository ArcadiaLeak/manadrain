module glslang.machine_independent.parse_context_base;
import glslang;

import std.format;

struct TPragma {
  this(bool o, bool d) { optimize = o; debug_ = d; }
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

  void delegate(int, int, bool, int, string) lineCallback;
  void delegate(int, const string[]) pragmaCallback;
  void delegate(int, int, string) versionCallback;
  void delegate(int, string, string) extensionCallback;
  void delegate(int, string) errorCallback;

  TVariable globalUniformBlock;
  uint globalUniformBinding;
  uint globalUniformSet;

  uint atomicCounterBlockSet;

  this(
    TSymbolTable symbolTable, TIntermediate interm, bool parsingBuiltins, int version_,
    EProfile profile, in SpvVersion, EShLanguage language,
    TInfoSink infoSink, bool forwardCompatible, EShMessages messages,
    string entryPoint = null
  ) {
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

    if (spvVersion.spv >= TARGET_SPV_1_3)
      intermediate.setUseStorageBuffer();
    
    if (entryPoint != null)
      sourceEntryPointName = entryPoint;
  }

  void setScanContext(TScanContext c) { scanContext = c; }
  void setPpContext(TPpContext c) { ppContext = c; }

  void setLineCallback(void delegate(int, int, bool, int, string) func) { lineCallback = func; }
  void setExtensionCallback(void delegate(int, string, string) func) { extensionCallback = func; }
  void setVersionCallback(void delegate(int, int, string) func) { versionCallback = func; }
  void setPragmaCallback(void delegate(int, const string[]) func) { pragmaCallback = func; }
  void setErrorCallback(void delegate(int, string) func) { errorCallback = func; }

  void outputMessage(Args...)(
    in TSourceLoc loc, string szReason, string szToken,
    string szExtraInfoFormat, TPrefixType prefix, Args args
  ) {
    infoSink.info.prefix = prefix;
    infoSink.info.location(loc, messages.MSG_DISPLAY_ERROR_COLUMN);
    infoSink.info.append = "'" ~ szToken ~ "' : " ~ szReason ~
      " " ~ format(szExtraInfoFormat, args) ~ "\n";

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
    if (messages.MSG_ONLY_PREPROCESSOR_BIT)
      return;
    if (messages.MSG_ENHANCED && numErrors > 0)
      return;

    outputMessage(
      loc, szReason, szToken, szExtraInfo,
      TPrefixType.EPrefixError
    );

    if (messages.MSG_CASCADING_ERRORS_BIT)
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

    if (messages.MSG_CASCADING_ERRORS_BIT)
      currentScanner.setEndOfInput;
  }

  void ppWarn(
    in TSourceLoc loc, string szReason,
    string szToken, string szExtraInfo
  ) {
    outputMessage(
      loc, szReason, szToken, szExtraInfo,
      TPrefixType.EPrefixWarning
    );
  }

  abstract void reservedPpErrorCheck(in TSourceLoc, string name, string op);
  abstract bool lineContinuationCheck(in TSourceLoc, bool endOfComment);
  abstract bool lineDirectiveShouldSetNextLine() const;

  TPpContext getPpContext() => ppContext;
  TScanContext getScanContext() => scanContext;
}
