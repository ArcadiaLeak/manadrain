module glslang.machine_independent.shader_lang;
import glslang;

enum EShOptimizationLevel {
  EShOptNoGeneration,
  EShOptNone,
  EShOptSimple,
  EShOptFull
}

struct TTarget {
  EShTargetLanguage language;
  uint version_;
}

struct TInputLanguage {
  EShLanguage stage;
  EShClient dialect;
  int dialectVersion;
  bool vulkanRulesRelaxed;
}

struct TClient {
  EShClient client;
  uint version_;
}

struct TEnvironment {
  TInputLanguage input;
  TClient client;
  TTarget target;
}

class TShader {
  static class Includer {}

  static class ForbidIncluder : Includer {}

  string[] strings;
  string[] stringNames;

  EShLanguage stage;
  int overrideVersion;

  TEnvironment environment;
  TInfoSink infoSink;
  TCompiler compiler;
  TIntermediate intermediate;

  this(EShLanguage s) {
    stage = s;

    infoSink = new TInfoSink;
    compiler = new TDeferredCompiler(stage, infoSink);
    intermediate = new TIntermediate(s);
  }

  void setStrings(string[] s) {
    strings = s;
  }

  void setEnvTarget(
    EShTargetLanguage lang,
    uint version_
  ) {
    environment.target.language = lang;
    environment.target.version_ = version_;
  }

  void setEnvInput(
    EShLanguage envStage,
    EShClient client,
    int version_
  ) {
    environment.input.stage = envStage;
    environment.input.dialect = client;
    environment.input.dialectVersion = version_;
  }

  void setEnvClient(
    EShClient client,
    uint version_
  ) {
    environment.client.client = client;
    environment.client.version_ = version_;
  }

  bool preprocess(
    const(TBuiltInResource)* builtInResources,
    int defaultVersion, EProfile defaultProfile,
    bool forceDefaultVersionAndProfile,
    bool forwardCompatible, EShMessages message,
    ref string output_string
  ) {
    return PreprocessDeferred(
      compiler, strings, stringNames, builtInResources,
      defaultVersion, defaultProfile, forceDefaultVersionAndProfile,
      overrideVersion, forwardCompatible, message, intermediate,
      output_string, environment
    );
  }
}

class TDeferredCompiler : TCompiler {
  this(EShLanguage s, TInfoSink i) {
    super(s, i);
  }
}

auto ppClosure(ref string outputString) {
  bool preprocess(
    TParseContextBase parseContext, TPpContext ppContext,
    TInputScanner input, bool versionWillBeError,
    TSymbolTable, TIntermediate, EShOptimizationLevel, EShMessages
  ) {
    import std.array;
    import std.container.dlist;
    import std.conv;

    enum dstring noNeededSpaceBeforeTokens = ";)[].,"d;
    enum dstring noNeededSpaceAfterTokens = ".(["d;

    TPpToken ppToken;

    parseContext.setScanner(input);
    ppContext.setInput(input, versionWillBeError);

    long lastSource = -1;
    long lastLine = 0;

    DList!string outputBuffer;

    bool syncToMostRecentString() {
      if (input.getLastValidSourceIndex != lastSource) {
        if (lastSource != -1 || lastLine != 0)
          outputBuffer ~= "\n";
        lastSource = input.getLastValidSourceIndex;
        lastLine = -1;
        return true;
      }
      return false;
    }

    bool syncToLine(long tokenLine) {
      syncToMostRecentString;
      const bool newLineStarted = lastLine < tokenLine;
      while (lastLine < tokenLine) {
        if (lastLine > 0) outputBuffer ~= "\n";
        ++lastLine;
      }
      return newLineStarted;
    }

    parseContext.setExtensionCallback = (
      int line, string extension,
      string behavior
    ) {
      syncToLine(line);
      outputBuffer ~= "#extension ";
      outputBuffer ~= extension;
      outputBuffer ~= " : ";
      outputBuffer ~= behavior;
    };

    parseContext.setLineCallback = (
      int curLineNum, int newLineNum, bool hasSource,
      int sourceNum, string sourceName
    ) {
      syncToLine(curLineNum);
      outputBuffer ~= "#line ";
      outputBuffer ~= newLineNum.to!string;
      if (hasSource) {
        outputBuffer ~= " ";
        if (sourceName !is null) {
          outputBuffer ~= "\"";
          outputBuffer ~= sourceName;
          outputBuffer ~= "\"";
        } else {
          outputBuffer ~= sourceNum.to!string;
        }
      }
      if (parseContext.lineDirectiveShouldSetNextLine)
        newLineNum -= 1;
      outputBuffer ~= "\n";
      lastLine = newLineNum + 1;
    };

    parseContext.setVersionCallback = (
      int line, int version_,
      string str
    ) {
      syncToLine(line);
      outputBuffer ~= "#version ";
      outputBuffer ~= version_.to!string;
      if (str) {
        outputBuffer ~= " ";
        outputBuffer ~= str;
      }
    };

    parseContext.setPragmaCallback = (
      int line, const string[] ops
    ) {
      syncToLine(line);
      outputBuffer ~= "#pragma ";
      foreach (op; ops) outputBuffer ~= op;
    };

    parseContext.setErrorCallback = (
      int line, string errorMessage
    ) {
      syncToLine(line);
      outputBuffer ~= "#error ";
      outputBuffer ~= errorMessage;
    };

    int lastToken = EndOfInput;
    string lastTokenName;
    do {
      int token = ppContext.tokenize(ppToken);
      if (token == EndOfInput)
        break;

      bool isNewString = syncToMostRecentString;
      bool isNewLine = syncToLine(ppToken.loc.line);

      if (isNewLine) {
        char[] buf = new char[ppToken.loc.column - 1];
        buf[] = ' ';
        outputBuffer ~= buf.idup;
      }
      
      if (!isNewString && !isNewLine && lastToken != EndOfInput) {
        import std.algorithm.searching;
        import std.range.primitives;
        if (token == '(') {
          if (
            lastToken != EFixedAtoms.PpAtomIdentifier ||
            lastTokenName == "if" ||
            lastTokenName == "for" ||
            lastTokenName == "while" ||
            lastTokenName == "switch"
          ) outputBuffer ~= " ";
        } else if (
          noNeededSpaceBeforeTokens.find(token).empty &&
          noNeededSpaceAfterTokens.find(lastToken).empty
        ) outputBuffer ~= " ";
      }
      if (token == EFixedAtoms.PpAtomIdentifier)
        lastTokenName = ppToken.nameStr;
      lastToken = token;
      if (token == EFixedAtoms.PpAtomConstString)
        outputBuffer ~= "\"";
      outputBuffer ~= ppToken.nameStr;
      if (token == EFixedAtoms.PpAtomConstString)
        outputBuffer ~= "\"";
    } while (true);
    outputBuffer ~= "\n";

    outputString = outputBuffer[].join;

    bool success = true;
    if (parseContext.getNumErrors > 0) {
      success = false;
      parseContext.infoSink.info.prefix = TPrefixType.EPrefixError;
      parseContext.infoSink.info.append = parseContext.getNumErrors.to!string
        ~ " compilation errors.  No code generated.\n\n";
    }
    return success;
  }

  return &preprocess;
}

bool PreprocessDeferred(
  TCompiler compiler,
  const string[] shaderStrings,
  const string[] stringNames,
  const TBuiltInResource* builtInResources,
  int defaultVersion,
  EProfile defaultProfile,
  bool forceDefaultVersionAndProfile,
  int overrideVersion,
  bool forwardCompatible,
  EShMessages messages,
  TIntermediate intermediate,
  ref string outputString,
  in TEnvironment environment
) {
  auto parser = outputString.ppClosure;
  return ProcessDeferred(
    compiler, shaderStrings, stringNames, builtInResources,
    defaultVersion, defaultProfile, forceDefaultVersionAndProfile,
    overrideVersion, forwardCompatible, messages, intermediate,
    parser, false, "", environment
  );
}

bool ProcessDeferred(ProcessingContext)(
  TCompiler compiler,
  const string[] shaderStrings,
  const string[] stringNames,
  const TBuiltInResource* builtInResources,
  int defaultVersion,
  EProfile defaultProfile,
  bool forceDefaultVersionAndProfile,
  int overrideVersion,
  bool forwardCompatible,
  EShMessages messages,
  TIntermediate intermediate,
  ProcessingContext processingContext,
  bool requireNonempty,
  string sourceEntryPointName,
  in TEnvironment environment
) {
  if (shaderStrings.length == 0)
    return true;

  const long numPre = 2;
  const long numPost = requireNonempty ? 1 : 0;
  const long numTotal = numPre + shaderStrings.length + numPost;

  import std.conv;
  import std.string;

  immutable(uint)[][] strings = new immutable(uint)[][numTotal];
  string[] names = new string[numTotal];

  foreach(s, str; shaderStrings)
    strings[s + numPre] = str.to!dstring.representation;

  if (stringNames !is null) {
    foreach(s, str; stringNames) {
      names[s + numPre] = str;
    }
  } else {
    foreach(s, str; shaderStrings) {
      names[s + numPre] = null;
    }
  }

  SpvVersion spvVersion;
  EShLanguage stage = compiler.getLanguage;
  TranslateEnvironment(environment, messages, stage, spvVersion);

  TInputScanner userInput = new TInputScanner(strings[numPre..$]);
  int version_ = 0;
  EProfile profile = EProfile(NO_PROFILE: 1);
  bool versionNotFirstToken = false;
  bool versionNotFirst = userInput.scanVersion(version_, profile, versionNotFirstToken);
  bool versionNotFound = version_ == 0;
  if (forceDefaultVersionAndProfile) {
    if (
      !messages.MSG_SUPPRESS_WARNINGS_BIT &&
      !versionNotFound &&
      (version_ != defaultVersion || profile != defaultProfile)
    ) {
      compiler.infoSink.info.append =
        "Warning, (version, profile) forced to be (" ~
        defaultVersion.to!string ~ ", " ~ defaultProfile.getName ~
        "), while in source code it is (" ~
        version_.to!string ~ ", " ~ profile.getName ~ ")\n";
    }

    if (versionNotFound) {
      versionNotFirstToken = false;
      versionNotFirst = false;
      versionNotFound = false;
    }
    version_ = defaultVersion;
    profile = defaultProfile;
  }
  if (overrideVersion != 0) {
    version_ = overrideVersion;
  }

  bool goodVersion = DeduceVersionProfile(
    compiler.infoSink, stage, versionNotFirst,
    defaultVersion, version_, profile, spvVersion
  );
  bool versionWillBeError = (
    versionNotFound ||
    (profile == EProfile(ES_PROFILE: 1) &&
      version_ >= 300 && versionNotFirst)
  );
  bool warnVersionNotFirst = false;
  if (!versionWillBeError && versionNotFirstToken) {
    if (messages.MSG_RELAXED_ERRORS_BIT)
      warnVersionNotFirst = true;
    else
      versionWillBeError = true;
  }

  intermediate.setVersion = version_;
  intermediate.setProfile = profile;
  intermediate.setSpv = spvVersion;
  RecordProcesses(intermediate, messages, sourceEntryPointName);
  if (spvVersion.vulkan > 0) intermediate.setOriginUpperLeft();

  if (messages.MSG_DEBUG_INFO_BIT) {
    intermediate.setSourceFile(names[numPre]);
    foreach (s; 0..shaderStrings.length)
      intermediate.addSourceText(strings[numPre + s]);
  }

  if (!SetupBuiltinSymbolTable(version_, profile, spvVersion))
    return false;

  TSymbolTable cachedTable = SharedSymbolTables
    [MapVersionToIndex(version_)][MapSpvVersionToIndex(spvVersion)]
    .MapEProfile(profile).MapEShLanguage(stage);

  TSymbolTable symbolTable = new TSymbolTable;

  TParseContextBase parseContext = CreateParseContext(
    symbolTable, intermediate, version_, profile, stage,
    compiler.infoSink, spvVersion, forwardCompatible, messages,
    false, sourceEntryPointName
  );
  TPpContext ppContext = new TPpContext(parseContext, names[numPre], null);

  TInputScanner fullInput = new TInputScanner(
    strings[numPre..$], names[numPre..$]);

  bool success = processingContext(
    parseContext, ppContext, fullInput,
    versionWillBeError, null, null, EShOptimizationLevel.init, messages
  );

  return success;
}

void TranslateEnvironment(
  in TEnvironment environment,
  ref EShMessages messages,
  ref EShLanguage stage,
  ref SpvVersion spvVersion
) {
  if (messages.MSG_SPV_RULES_BIT)
    spvVersion.spv = TARGET_SPV_1_0;
  if (messages.MSG_VULKAN_RULES_BIT) {
    spvVersion.vulkan = TARGET_VULKAN_1_0;
    spvVersion.vulkanGlsl = 100;
  } else if (spvVersion.spv != 0)
    spvVersion.openGl = 100;

  stage = environment.input.stage;
  final switch (environment.input.dialect) {
    case EShClient.CLIENT_NONE:
      break;
    case EShClient.CLIENT_VULKAN:
      spvVersion.vulkanGlsl = environment.input.dialectVersion;
      spvVersion.vulkanRelaxed = environment.input.vulkanRulesRelaxed;
      break;
    case EShClient.CLIENT_OPENGL:
      spvVersion.openGl = environment.input.dialectVersion;
  }

  if (environment.client.client == EShClient.CLIENT_VULKAN)
    spvVersion.vulkan = environment.client.version_;

  if (environment.target.language == EShTargetLanguage.TARGET_SPV)
    spvVersion.spv = environment.target.version_;
}

bool DeduceVersionProfile(
  TInfoSink infoSink,
  EShLanguage stage,
  bool versionNotFirst,
  int defaultVersion,
  ref int version_,
  ref EProfile profile,
  in SpvVersion spvVersion
) {
  const int FirstProfileVersion = 150;
  bool correct = true;

  if (version_ == 0) {
    version_ = defaultVersion;
  }

  if (profile == EProfile(NO_PROFILE: 1)) {
    if (version_ == 300 || version_ == 310 || version_ == 320) {
      correct = false;
      infoSink.info.message(
        TPrefixType.EPrefixError,
        "#version: versions 300, 310, and 320 require specifying the 'es' profile"
      );
      profile = EProfile(ES_PROFILE: 1);
    } else if (version_ == 100)
      profile = EProfile(ES_PROFILE: 1);
    else if (version_ >= FirstProfileVersion)
      profile = EProfile(CORE_PROFILE: 1);
    else
      profile = EProfile(NO_PROFILE: 1);
  } else {
    if (version_ < 150) {
      correct = false;
      infoSink.info.message(
        TPrefixType.EPrefixError,
        "#version: versions before 150 do not allow a profile token"
      );
      if (version_ == 100)
        profile = EProfile(ES_PROFILE: 1);
      else
        profile = EProfile(NO_PROFILE: 1);
    } else if (version_ == 300 || version_ == 310 || version_ == 320) {
      if (profile != EProfile(ES_PROFILE: 1)) {
        correct = false;
        infoSink.info.message(
          TPrefixType.EPrefixError,
          "#version: versions 300, 310, and 320 support only the es profile"
        );
      }
      profile = EProfile(ES_PROFILE: 1);
    } else {
      if (profile == EProfile(ES_PROFILE: 1)) {
        correct = false;
        infoSink.info.message(
          TPrefixType.EPrefixError,
          "#version: only version 300, 310, and 320 support the es profile"
        );
        if (version_ >= FirstProfileVersion)
          profile = EProfile(CORE_PROFILE: 1);
        else
          profile = EProfile(NO_PROFILE: 1);
      }
    }
  }

  switch (version_) {
    case 100: break;
    case 300: break;
    case 310: break;
    case 320: break;

    case 110: break;
    case 120: break;
    case 130: break;
    case 140: break;
    case 150: break;
    case 330: break;
    case 400: break;
    case 410: break;
    case 420: break;
    case 430: break;
    case 440: break;
    case 450: break;
    case 460: break;

    default:
      correct = false;
      infoSink.info.message(TPrefixType.EPrefixError, "version not supported");
      if (profile == EProfile(ES_PROFILE: 1))
        version_ = 310;
      else {
        version_ = 450;
        profile = EProfile(CORE_PROFILE: 1);
      }
      break;
  }

  if (
    profile == EProfile(ES_PROFILE: 1) &&
    version_ >= 300 && versionNotFirst
  ) {
    correct = false;
    infoSink.info.message(
      TPrefixType.EPrefixError,
      "#version: statement must appear first in es-profile shader; before comments or newlines"
    );
  }

  if (spvVersion.spv != 0) {
    if (profile == EProfile(ES_PROFILE: 1)) {
      if (version_ < 310) {
        correct = false;
        infoSink.info.message(
          TPrefixType.EPrefixError,
          "#version: ES shaders for SPIR-V require version 310 or higher"
        );
        version_ = 310;
      }
    } else if (profile == EProfile(COMPATIBILITY_PROFILE: 1)) {
      infoSink.info.message(
        TPrefixType.EPrefixError,
        "#version: compilation for SPIR-V does not support the compatibility profile"
      );
    } else {
      if (spvVersion.vulkan > 0 && version_ < 140) {
        correct = false;
        infoSink.info.message(
          TPrefixType.EPrefixError,
          "#version: Desktop shaders for Vulkan SPIR-V require version 140 or higher"
        );
        version_ = 140;
      }
      if (spvVersion.openGl >= 100 && version_ < 330) {
        correct = false;
        infoSink.info.message(
          TPrefixType.EPrefixError,
          "#version: Desktop shaders for OpenGL SPIR-V require version 330 or higher"
        );
      }
    }
  }

  return correct;
}

void RecordProcesses(
  TIntermediate intermediate,
  EShMessages messages,
  string sourceEntryPointName
) {
  if (messages.MSG_RELAXED_ERRORS_BIT)
    intermediate.addProcess = "relaxed-errors";
  if (messages.MSG_SUPPRESS_WARNINGS_BIT)
    intermediate.addProcess = "suppress-warnings";
  if (messages.MSG_KEEP_UNCALLED_BIT)
    intermediate.addProcess = "keep-uncalled";
  if (sourceEntryPointName.length > 0) {
    intermediate.addProcess = "source-entrypoint";
    intermediate.addProcessArgument = sourceEntryPointName;
  }
}

bool SetupBuiltinSymbolTable(
  int version_,
  EProfile profile,
  in SpvVersion spvVersion
) {
  TInfoSink infoSink = new TInfoSink;

  int versionIndex = MapVersionToIndex(version_);
  int spvVersionIndex = MapSpvVersionToIndex(spvVersion);

  TSymbolTable specific = CommonSymbolTable
    [versionIndex][spvVersionIndex].MapEProfile(profile)
    .CLASS_GENERAL;
  if (specific !is null) return true;

  TPrecisionClass!TSymbolTable commonTable;
  TShLanguage!TSymbolTable stageTables;
  static foreach (precClass; 0..commonTable.tupleof.length)
    commonTable.tupleof[precClass] = new TSymbolTable;
  static foreach (stage; 0..stageTables.tupleof.length)
    stageTables.tupleof[stage] = new TSymbolTable;
  
  bool success = InitializeSymbolTables(
    infoSink, commonTable, stageTables,
    version_, profile, spvVersion
  );
  if (!success) return false;

  static foreach (precClass; 0..commonTable.tupleof.length) {
    if (!commonTable.tupleof[precClass].isEmpty) {
      CommonSymbolTable[versionIndex][spvVersionIndex]
        .MapEProfile(profile).tupleof[precClass] =
          new TSymbolTable;

      CommonSymbolTable[versionIndex][spvVersionIndex]
        .MapEProfile(profile).tupleof[precClass]
          .copyTable(commonTable.tupleof[precClass]);

      assert(0);
    }
  }

  return true;
}

enum int VersionCount = 17;

int MapVersionToIndex(int version_) {
  int index = 0;

  if (version_ == 100) index = 0;
  else if (version_ == 110) index = 1;
  else if (version_ == 120) index = 2;
  else if (version_ == 130) index = 3;
  else if (version_ == 140) index = 4;
  else if (version_ == 150) index = 5;
  else if (version_ == 300) index = 6;
  else if (version_ == 330) index = 7;
  else if (version_ == 400) index = 8;
  else if (version_ == 410) index = 9;
  else if (version_ == 420) index = 10;
  else if (version_ == 430) index = 11;
  else if (version_ == 440) index = 12;
  else if (version_ == 310) index = 13;
  else if (version_ == 450) index = 14;
  else if (version_ == 500) index = 0;
  else if (version_ == 320) index = 15;
  else if (version_ == 460) index = 16;
  else index = VersionCount;

  if (index >= VersionCount)
    throw new Exception(
      "MapVersionToIndex failed!"
    );
  
  return index;
}

enum int SpvVersionCount = 4;

int MapSpvVersionToIndex(in SpvVersion spvVersion) {
  int index = 0;

  if (spvVersion.openGl > 0)
    index = 1;
  else if (spvVersion.vulkan > 0) {
    if (!spvVersion.vulkanRelaxed)
      index = 2;
    else
      index = 3;
  }

  if (index >= SpvVersionCount)
    throw new Exception(
      "MapSpvVersionToIndex failed!"
    );

  return index;
}

struct TProfile(T) {
  T NO_PROFILE;
  T CORE_PROFILE;
  T COMPATIBILITY_PROFILE;
  T ES_PROFILE;

  ref T MapEProfile(EProfile profile) {
    if (profile == EProfile(NO_PROFILE: 1))
      return NO_PROFILE;
    else if (profile == EProfile(CORE_PROFILE: 1))
      return CORE_PROFILE;
    else if (profile == EProfile(COMPATIBILITY_PROFILE: 1))
      return COMPATIBILITY_PROFILE;
    if (profile == EProfile(ES_PROFILE: 1))
      return ES_PROFILE;
    else throw new Exception("MapEProfile failed!");
  }
}

struct TPrecisionClass(T) {
  TSymbolTable CLASS_GENERAL;
  TSymbolTable CLASS_FRAGMENT;
}

struct TShLanguage(T) {
  T STAGE_VERTEX;
  T STAGE_TESSCONTROL;
  T STAGE_TESSEVALUATION;
  T STAGE_GEOMETRY;
  T STAGE_FRAGMENT;
  T STAGE_COMPUTE;
  T STAGE_RAYGEN;
  T STAGE_INTERSECT;
  T STAGE_ANYHIT;
  T STAGE_CLOSESTHIT;
  T STAGE_MISS;
  T STAGE_CALLABLE;
  T STAGE_TASK;
  T STAGE_MESH;

  ref T MapEShLanguage(EShLanguage language) {
    final switch (language) {
      case EShLanguage.STAGE_VERTEX:
        return STAGE_VERTEX;
      case EShLanguage.STAGE_TESSCONTROL:
        return STAGE_TESSCONTROL;
      case EShLanguage.STAGE_TESSEVALUATION:
        return STAGE_TESSEVALUATION;
      case EShLanguage.STAGE_GEOMETRY:
        return STAGE_GEOMETRY;
      case EShLanguage.STAGE_FRAGMENT:
        return STAGE_FRAGMENT;
      case EShLanguage.STAGE_COMPUTE:
        return STAGE_COMPUTE;
      case EShLanguage.STAGE_RAYGEN:
        return STAGE_RAYGEN;
      case EShLanguage.STAGE_INTERSECT:
        return STAGE_INTERSECT;
      case EShLanguage.STAGE_ANYHIT:
        return STAGE_ANYHIT;
      case EShLanguage.STAGE_CLOSESTHIT:
        return STAGE_CLOSESTHIT;
      case EShLanguage.STAGE_MISS:
        return STAGE_MISS;
      case EShLanguage.STAGE_CALLABLE:
        return STAGE_CALLABLE;
      case EShLanguage.STAGE_TASK:
        return STAGE_TASK;
      case EShLanguage.STAGE_MESH:
        return STAGE_MESH;
    }
  }
}

TProfile!(TPrecisionClass!TSymbolTable)
  [SpvVersionCount][VersionCount] CommonSymbolTable;

TProfile!(TShLanguage!TSymbolTable)
  [SpvVersionCount][VersionCount] SharedSymbolTables;

TParseContextBase CreateParseContext(
  TSymbolTable symbolTable, TIntermediate intermediate,
  int version_, EProfile profile, EShLanguage language, TInfoSink infoSink,
  in SpvVersion spvVersion,bool forwardCompatible, EShMessages messages,
  bool parsingBuiltIns, string sourceEntryPointName = ""
) {
  if (sourceEntryPointName.length == 0)
    intermediate.setEntryPointName = "main";
  auto parseContext = new TParseContext(
    symbolTable, intermediate, parsingBuiltIns, version_, profile, spvVersion,
    language, infoSink, forwardCompatible, messages, sourceEntryPointName
  );
  return parseContext;
}

bool InitializeSymbolTables(
  TInfoSink infoSink, TPrecisionClass!TSymbolTable commonTable,
  TShLanguage!TSymbolTable symbolTables, int version_, EProfile profile,
  in SpvVersion spvVersion
) {
  bool success = true;
  TBuiltIns builtInParseables = new TBuiltIns();

  if (builtInParseables is null) return false;

  builtInParseables.initialize(version_, profile, spvVersion);

  success &= InitializeSymbolTable(
    builtInParseables.getCommonString,
    version_, profile, spvVersion, EShLanguage.STAGE_VERTEX,
    infoSink, commonTable.CLASS_GENERAL
  );

  return success;
}

bool InitializeSymbolTable(
  string builtIns, int version_, EProfile profile,
  in SpvVersion spvVersion, EShLanguage language,
  TInfoSink infoSink, TSymbolTable symbolTable
) {
  TIntermediate intermediate = new TIntermediate(language, version_, profile);
  TParseContextBase parseContext = CreateParseContext(
    symbolTable, intermediate, version_, profile, language,
    infoSink, spvVersion, true, EShMessages(), true
  );

  TShader.ForbidIncluder includer = new TShader.ForbidIncluder;
  TPpContext ppContext = new TPpContext(parseContext, "", includer);
  TScanContext scanContext = new TScanContext(parseContext);
  parseContext.setScanContext = scanContext;
  parseContext.setPpContext = ppContext;

  symbolTable.push;

  import std.conv;
  import std.string;

  immutable(uint)[][] builtInsArr = [builtIns.to!dstring.representation];

  if (builtInsArr[0].length == 0)
    return true;

  TInputScanner input = new TInputScanner(builtInsArr);

  return true;
}
