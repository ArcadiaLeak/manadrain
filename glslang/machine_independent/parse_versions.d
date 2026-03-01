module glslang.machine_independent.parse_versions;
import glslang;

class TParseVersions {
  bool forwardCompatible;
  EProfile profile;

  TInfoSink infoSink;

  int version_;
  EShLanguage language;
  SpvVersion spvVersion;
  TIntermediate intermediate;

  EShMessages messages;
  int numErrors;
  TInputScanner currentScanner;
  TExtensionBehavior[string] extensionBehavior;

  this(
    TIntermediate interm, int version_, EProfile profile,
    in SpvVersion spvVersion, EShLanguage language, TInfoSink infoSink,
    bool forwardCompatible, EShMessages messages
  ) {
    this.forwardCompatible = forwardCompatible; this.profile = profile;
    this.infoSink = infoSink; this.version_ = version_; this.language = language;
    this.spvVersion = spvVersion; intermediate = interm; this.messages = messages;
    numErrors = 0; currentScanner = null;
  }

  bool isEsProfile() const => profile == EProfile(ES_PROFILE: 1);

  int getNumErrors() const => numErrors;

  bool relaxedErrors() const => messages.MSG_RELAXED_ERRORS_BIT;

  ref const(TSourceLoc) getCurrentLoc() const => currentScanner.getSourceLoc();

  void setScanner(TInputScanner scanner) { currentScanner = scanner; }

  bool extensionTurnedOn(string extension) {
    switch (getExtensionBehavior(extension)) {
      case TExtensionBehavior.EBhEnable:
      case TExtensionBehavior.EBhRequire:
      case TExtensionBehavior.EBhWarn:
        return true;
      default:
        break;
    }

    return false;
  }

  TExtensionBehavior getExtensionBehavior(string extension) {
    if (auto ebhptr = extension in extensionBehavior) {
      return *ebhptr;
    } else {
      return TExtensionBehavior.EBhMissing;
    }
  }

  void ppRequireExtensions(
    in TSourceLoc loc,
    const string[] extensions, string featureDesc
  ) {
    if (checkExtensionsRequested(loc, extensions, featureDesc))
      return;

    if (extensions.length == 1)
      ppError(loc, "required extension not requested:", featureDesc, extensions[0]);
    else {
      ppError(loc, "required extension not requested:", featureDesc, "Possible extensions include:");
      foreach (ext; extensions)
        infoSink.info.message(TPrefixType.EPrefixNone, ext);
    }
  }

  bool checkExtensionsRequested(in TSourceLoc loc, const string[] extensions, string featureDesc) {
    foreach (ext; extensions) {
      TExtensionBehavior behavior = getExtensionBehavior(ext);
      if (behavior == TExtensionBehavior.EBhEnable || behavior == TExtensionBehavior.EBhRequire)
        return true;
    }

    bool warned = false;
    foreach (ext; extensions) {
      TExtensionBehavior behavior = getExtensionBehavior(ext);
      if (behavior == TExtensionBehavior.EBhDisable && relaxedErrors) {
        infoSink.info.message(
          TPrefixType.EPrefixWarning,
          "The following extension must be enabled to use this feature:",
          loc, messages.MSG_ABSOLUTE_PATH, messages.MSG_DISPLAY_ERROR_COLUMN
        );
        behavior = TExtensionBehavior.EBhWarn;
      }
      if (behavior == TExtensionBehavior.EBhWarn) {
        infoSink.info.message(
          TPrefixType.EPrefixWarning,
          "extension " ~ ext ~ " is being used for " ~ featureDesc,
          loc, messages.MSG_ABSOLUTE_PATH, messages.MSG_DISPLAY_ERROR_COLUMN
        );
        warned = true;
      }
    }
    if (warned)
      return true;
    return false;
  }

  abstract void warn(
    in TSourceLoc loc, string szReason,
    string szToken, string szExtraInfo
  );

  abstract void error(
    in TSourceLoc loc, string szReason,
    string szToken, string szExtraInfo
  );

  abstract void ppError(
    in TSourceLoc loc, string szReason,
    string szToken, string szExtraInfo
  );

  abstract void ppWarn(
    in TSourceLoc loc, string szReason,
    string szToken, string szExtraInfo
  );

  void requireProfile(in TSourceLoc loc, EProfile profileMask, string featureDesc) {
    if (!(profile & profileMask))
      error(loc, "not supported with this profile:", featureDesc, profile.getName);
  }

  void profileRequires(
    in TSourceLoc loc, EProfile profileMask, int minVersion, string extension,
    string featureDesc
  ) {
    profileRequires(
      loc, profileMask, minVersion,
      extension ? [extension] : null, featureDesc
    );
  }

  void profileRequires(
    in TSourceLoc loc, EProfile profileMask, int minVersion, string[] extensions,
    string featureDesc
  ) {
    if (profile & profileMask) {
      bool okay = minVersion > 0 && version_ >= minVersion;
      foreach (ext; extensions) {
        switch (getExtensionBehavior(ext)) {
          case TExtensionBehavior.EBhWarn:
            infoSink.info.message(
              TPrefixType.EPrefixWarning,
              "extension " ~ ext ~ " is being used for " ~ featureDesc,
              loc, messages.MSG_ABSOLUTE_PATH, messages.MSG_DISPLAY_ERROR_COLUMN
            );
            goto case;
          case TExtensionBehavior.EBhRequire:
          case TExtensionBehavior.EBhEnable:
            okay = true;
            break;
          default: break;
        }
      }
      if (!okay)
        error(loc, "not supported for this version or the enabled extensions",
          featureDesc, "");
    }
  }
}
