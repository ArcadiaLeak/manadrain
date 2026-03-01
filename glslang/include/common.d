module glslang.include.common;

struct TSourceLoc {
  string name;
  long string_;
  long line;
  long column;

  void clear() {
    name = null;
    string_ = 0;
    line = 0;
    column = 0;
  }

  void clear(long stringNum) {
    clear;
    string_ = stringNum;
  }

  string getStringNameOrNum(bool quoteStringName = true) const {
    import std.conv;
    if (name !is null)
      return quoteStringName ? ("\"" ~ name ~ "\"") : name;
    return string_.to!string;
  }
}

struct TPragmaTable {}
