module glslang.machine_independent.parse_versions;

import glslang;

import std.conv;

class TParseVersions {
  bool forwardCompatible;
  profile_t profile;

  TInfoSink infoSink;

  int version_;
  EShLanguage language;
  SpvVersion spvVersion;
  TIntermediate intermediate;

  messages_t messages;
  int numErrors;
  TInputScanner currentScanner;
  TExtensionBehavior[string] extensionBehavior;

  this(
    TIntermediate interm, int version_, profile_t profile,
    in SpvVersion spvVersion, EShLanguage language, TInfoSink infoSink,
    bool forwardCompatible, messages_t messages
  ) {
    this.forwardCompatible = forwardCompatible; this.profile = profile;
    this.infoSink = infoSink; this.version_ = version_; this.language = language;
    this.spvVersion = spvVersion; intermediate = interm; this.messages = messages;
    numErrors = 0; currentScanner = null;
  }

  bool isEsProfile() const {
    return profile == profile_t.ES_PROFILE;
  }

  bool relaxedErrors() const {
    return (messages & messages_t.MSG_RELAXED_ERRORS_BIT) != 0;
  }

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

  void profileRequires(
    in TSourceLoc loc, int profileMask, int minVersion, string extension,
    string featureDesc
  ) {
    profileRequires(
      loc, profileMask, minVersion,
      extension ? [extension] : null, featureDesc
    );
  }

  ref const(TSourceLoc) getCurrentLoc() const =>
    currentScanner.getSourceLoc();

  abstract void warn(
    in TSourceLoc loc, string szReason,
    string szToken, string szExtraInfo
  );

  abstract void error(
    in TSourceLoc loc, string szReason,
    string szToken, string szExtraInfo
  );

  void profileRequires(
    in TSourceLoc loc, int profileMask, int minVersion, string[] extensions,
    string featureDesc
  ) {
    if (profile & profileMask) {
      bool okay = minVersion > 0 && version_ >= minVersion;
      foreach (ext; extensions) {
        switch (getExtensionBehavior(ext)) {
          case TExtensionBehavior.EBhWarn:
            infoSink.info.message(
              TPrefixType.EPrefixWarning,
              i"extension $(ext) is being used for $(featureDesc)".text,
              loc,
              cast(bool) (messages & messages_t.MSG_DISPLAY_ERROR_COLUMN)
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

  void setScanner(TInputScanner scanner) { currentScanner = scanner; }
}
