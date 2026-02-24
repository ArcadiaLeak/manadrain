module glslang.include.common;

enum int MaxTokenLength = 1024;

struct TSourceLoc {
  string name;
  int string_;
  int line;
  int column;

  void init() {
    name = null;
    string_ = 0;
    line = 0;
    column = 0;
  }

  void init(int stringNum) {
    init();
    string_ = stringNum;
  }
}

struct TPragmaTable {}
