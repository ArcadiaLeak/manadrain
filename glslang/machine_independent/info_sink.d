module glslang.machine_independent.info_sink;

import glslang;

import std.format;

enum TPrefixType {
  EPrefixNone,
  EPrefixWarning,
  EPrefixError,
  EPrefixInternalError,
  EPrefixUnimplemented,
  EPrefixNote
}

struct TInfoSinkBase {
  string sink;
  string shaderFileName;

  void append(const(char[]) s) {
    sink ~= s;
  }

  void message(TPrefixType msg, string s) {
    prefix(msg);
    append(s);
    append("\n");
  }

  void message(
    TPrefixType msg, string s, in TSourceLoc loc,
    bool absolute, bool displayColumn
  ) {
    prefix(msg);
    location(loc, absolute, displayColumn);
    append(s);
    append("\n");
  }

  void prefix(TPrefixType message) {
    final switch (message) {
      case TPrefixType.EPrefixNone:
        break;
      case TPrefixType.EPrefixWarning:
        append("WARNING: ");
        break;
      case TPrefixType.EPrefixError:
        append("ERROR: ");
        break;
      case TPrefixType.EPrefixInternalError:
        append("INTERNAL ERROR: ");
        break;
      case TPrefixType.EPrefixUnimplemented:
        append("UNIMPLEMENTED: ");
        break;
      case TPrefixType.EPrefixNote:
        append("NOTE: ");
        break;
    }
  }

  void location(in TSourceLoc loc, bool absolute = false, bool displayColumn = false) {
    string locText = loc.name;
    if (displayColumn) {
      locText ~= format(":%d:%d", loc.line, loc.column);
    } else {
      locText ~= format(":%d", loc.line);
    }

    append(locText);
    append(": ");
  }
}

class TInfoSink {
  TInfoSinkBase info;
}
