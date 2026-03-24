module glslang.include.sh_handle;

import glslang;

class TCompiler {
  EShLanguage getLanguage() => language;

  this(EShLanguage l, TInfoSink i) {
    language = l;
  }

  TInfoSink infoSink;
  EShLanguage language;
}
