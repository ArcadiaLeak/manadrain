module quickjs;

import std.container.dlist;

enum JSGCPhase {
  NONE,
  DECREF,
  REMOVE_CYCLES
}

enum JSWknownAtom : uint {
  @("null") NULL, @("false") FALSE, @("true") TRUE, @("if") IF,
  @("else") ELSE, @("return") RETURN, @("var") VAR, @("this") THIS,
  @("delete") DELETE, @("void") VOID, @("typeof") TYPEOF, @("new") NEW,
  @("in") IN, @("instanceof") INSTANCEOF, @("do") DO, @("while") WHILE,
  @("for") FOR, @("break") BREAK, @("continue") CONTINUE, @("switch") SWITCH,
  @("case") CASE, @("default") DEFAULT, @("throw") THROW, @("try") TRY,
  @("catch") CATCH, @("finally") FINALLY, @("function") FUNCTION,
  @("debugger") DEBUGGER, @("with") WITH, @("class") CLASS, @("const") CONST,
  @("enum") ENUM, @("export") EXPORT, @("extends") EXTENDS, @("import") IMPORT,
  @("super") SUPER, @("implements") IMPLEMENTS, @("interface") INTERFACE,
  @("let") LET, @("package") PACKAGE, @("private") PRIVATE,
  @("protected") PROTECTED, @("public") PUBLIC, @("static") STATIC,
  @("yield") YIELD, @("await") AWAIT
}

class JSRuntime {
  DList!size_t context_list;
  DList!size_t gc_obj_list;
  DList!size_t gc_zero_ref_count_list;
  DList!size_t tmp_obj_list;
  size_t malloc_gc_threshold = 256 * 1024;
  DList!size_t weakref_list;

  int InitAtoms() {
    assert(0);
  }
}

void main(string[] args) {
  
}
