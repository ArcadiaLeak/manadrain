module lua;

public import lua.lauxlib;
public import lua.lstate;
public import lua.lzio;

alias lua_Reader = string function(lua_State L, string ud);

void main(string[] args) {
  import std.stdio;
  import std.path;
  import std.file;

  string luaString = args[args.length - 1]
    .buildNormalizedPath("additive_expression.lua")
    .readText;

  writeln(luaString);
}
