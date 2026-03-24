module glslang.machine_independent.scan;
import glslang;

enum int EndOfInput = -1;

class TInputScanner {
  immutable(uint)[][] sources;
  size_t currentSource;
  size_t currentChar;

  TSourceLoc[] loc;

  int stringBias;
  int finale;

  TSourceLoc logicalSourceLoc;
  bool singleLogical;

  bool endOfFileReached;

  this(
    immutable(uint)[][] s, string[] names = null,
    int b = 0, int f = 0, bool single = false
  ) {
    sources = s;
    currentSource = 0;
    currentChar = 0;
    stringBias = b;
    finale = f;
    singleLogical = single;
    endOfFileReached = false;

    loc = new TSourceLoc[sources.length];
    for (int i = 0; i < sources.length; ++i) {
      loc[i].clear(i - stringBias);
    }
    if (names !is null)
      foreach (i; 0..sources.length)
        loc[i].name = names[i];
    loc[currentSource].line = 1;
    logicalSourceLoc.clear(1);
    logicalSourceLoc.name = loc[0].name;
  }

  bool scanVersion(
    out int version_,
    out EProfile profile,
    out bool notFirstToken
  ) {
    bool versionNotFirst = false;
    notFirstToken = false;
    version_ = 0;
    profile = EProfile(NO_PROFILE: 1);

    bool foundNonSpaceTab = false;
    bool lookingInMiddle = false;
    uint c;
    do {
      if (lookingInMiddle) {
        notFirstToken = true;
        if (peek != '\n' && peek != '\r') {
          do c = get; while (
            c != EndOfInput &&
            c != '\n' &&
            c != '\r'
          );
        }
        while (peek == '\n' || peek == '\r')
          get;
        if (peek == EndOfInput)
          return true;
      }
      lookingInMiddle = true;

      consumeWhitespaceComment(foundNonSpaceTab);
      if (foundNonSpaceTab) versionNotFirst = true;

      if (get != '#') {
        versionNotFirst = true;
        continue;
      }

      do c = get; while (c == ' ' || c == '\t');

      if (
        c != 'v' ||
        get != 'e' ||
        get != 'r' ||
        get != 's' ||
        get != 'i' ||
        get != 'o' ||
        get != 'n'
      ) {
        versionNotFirst = true;
        continue;
      }

      do c = get; while (c == ' ' || c == '\t');

      while (c >= '0' && c <= '9') {
        version_ = 10 * version_ + (c - '0');
        c = get;
      }
      if (version_ == 0) {
        versionNotFirst = true;
        continue;
      }

      while (c == ' ' || c == '\t') c = get;

      import std.array;
      import std.container.dlist;
      import std.conv;
      import std.string;
      
      DList!uint profileCharList;
      while(
        c != EndOfInput &&
        c != ' ' && c != '\t' &&
        c != '\n' && c != '\r'
      ) {
        profileCharList ~= c;
        c = get;
      }
      if (c != EndOfInput && c != ' ' && c != '\t' && c != '\n' && c != '\r') {
        versionNotFirst = true;
        continue;
      }

      string profileString = profileCharList[].array.assumeUTF.to!string;
      if (profileString == "es")
        profile = EProfile(ES_PROFILE: 1);
      else if (profileString == "core")
        profile = EProfile(CORE_PROFILE: 1);
      else if (profileString == "compatibility")
        profile = EProfile(COMPATIBILITY_PROFILE: 1);

      return versionNotFirst;
    } while (true);
  }

  void consumeWhitespaceComment(out bool foundNonSpaceTab) {
    do {
      consumeWhiteSpace(foundNonSpaceTab);

      uint c = peek;
      if (c != '/' || c == EndOfInput)
        return;

      foundNonSpaceTab = true;
      if (!consumeComment)
        return;
    } while (true);
  }

  void consumeWhiteSpace(out bool foundNonSpaceTab) {
    uint c = peek;
    while (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      if (c == '\r' || c == '\n') foundNonSpaceTab = true;
      get;
      c = peek;
    }
  }

  bool consumeComment() {
    if (peek != '/') return false;

    get;
    uint c = peek;
    if (c == '/') {
      get;
      c = get;
      do {
        while (c != EndOfInput && c != '\\' && c != '\r' && c != '\n')
          c = get;

        if (c == EndOfInput || c == '\r' || c == '\n') {
          while (c == '\r' || c == '\n') c = get;

          break;
        } else {
          c = get;

          if (c == '\r' && peek == '\n')
            get;
          c = get;
        }
      } while (true);

      if (c != EndOfInput) unget;

      return true;
    } else if (c == '*') {
      get;
      c = get;
      do {
        while (c != EndOfInput && c != '*') c = get;
        if (c == '*') {
          c = get;
          if (c == '/') break;
        } else break;
      } while (true);
    
      return true;
    } else {
      unget;

      return false;
    }
  }

  uint peek() {
    if (currentSource >= sources.length) {
      endOfFileReached = true;
      return EndOfInput;
    }

    size_t sourceToRead = currentSource;
    size_t charToRead = currentChar;
    while (charToRead >= sources[sourceToRead].length) {
      charToRead = 0;
      sourceToRead += 1;
      if (sourceToRead >= sources.length) {
        return EndOfInput;
      }
    }

    return sources[sourceToRead][charToRead];
  }

  uint get() {
    uint ret = peek;
    if (ret == EndOfInput)
      return ret;

    ++loc[currentSource].column;
    ++logicalSourceLoc.column;
    if (ret == '\n') {
      ++loc[currentSource].line;
      ++logicalSourceLoc.line;
      logicalSourceLoc.column = 0;
      loc[currentSource].column = 0;
    }
    advance();

    return ret;
  }

  ref const(TSourceLoc) getSourceLoc() const {
    if (singleLogical) {
      return logicalSourceLoc;
    } else {
      import std.algorithm.comparison;
      return loc[max(0, min(currentSource, sources.length - finale - 1))];
    }
  }

  void setEndOfInput() {
    endOfFileReached = true;
    currentSource = sources.length;
  }

  void unget() {
    if (endOfFileReached) return;

    if (currentChar > 0) {
      --currentChar;
      --loc[currentSource].column;
      --logicalSourceLoc.column;
      if (loc[currentSource].column < 0) {
        size_t chIndex = currentChar;
        while (chIndex > 0) {
          if (sources[currentSource][chIndex] == '\n') {
            break;
          }
          --chIndex;
        }
        logicalSourceLoc.column = currentChar - chIndex;
        loc[currentSource].column = currentChar - chIndex;
      }
    } else {
      do {
        --currentSource;
      } while (currentSource > 0 && sources[currentSource].length == 0);
      if (sources[currentSource].length == 0)
        currentChar = 0;
      else
        currentChar = sources[currentSource].length - 1;
    }
    if (peek == '\n') {
      --loc[currentSource].line;
      --logicalSourceLoc.line;
    }
  }

  void advance() {
    ++currentChar;
    if (currentChar >= sources[currentSource].length) {
      ++currentSource;
      if (currentSource < sources.length) {
        loc[currentSource].string_ = loc[currentSource - 1].string_ + 1;
        loc[currentSource].line = 1;
        loc[currentSource].column = 0;
      }
      while (currentSource < sources.length && sources[currentSource].length == 0) {
        ++currentSource;
        if (currentSource < sources.length) {
          loc[currentSource].string_ = loc[currentSource - 1].string_ + 1;
          loc[currentSource].line = 1;
          loc[currentSource].column = 0;
        }
      }
      currentChar = 0;
    }
  }

  size_t getLastValidSourceIndex() const {
    import std.algorithm.comparison;
    return min(currentSource, sources.length - 1);
  }
}

class TParserToken {}
