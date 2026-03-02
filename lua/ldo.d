module lua.ldo;
import lua;

class SParser {
  ZIO z;
  string buff;
}

ubyte luaD_protectedparser(
  lua_State L, ZIO z,
  string name, string mode
) {
  assert(0);
}
