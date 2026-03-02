module lua.lauxlib;
import lua;

int luaL_loadbufferx(lua_State L, string buff, string name, string mode) =>
  L.lua_load(&getS, buff, name, mode);

string getS(lua_State L, string ls) => ls;

int lua_load(
  lua_State L, lua_Reader reader, string data,
  string chunkname, string mode
) {
  ZIO z = new ZIO(L, reader, data);
  ubyte status;
  if (!chunkname) chunkname = "?";
  status = luaD_protectedparser(L, z, chunkname, mode);

  assert(0);
}

