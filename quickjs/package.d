module quickjs;

public import quickjs.js_atom_enum;
public import quickjs.js_class_enum;
public import quickjs.js_function_def;
public import quickjs.js_machine;
public import quickjs.js_parse_state;
public import quickjs.js_runtime;
public import quickjs.js_token;
public import quickjs.js_value;
public import quickjs.regexp;
public import quickjs.unicode;

void main(string[] args) {
  import std.file;
  import std.stdio;
  args[$ - 1].readText.writeln;

  JSRuntime rt = new JSRuntime;
  rt.insert_wellknown;
}
