module lua.lzio;
import lua;

class ZIO {
  lua_State L;
  lua_Reader reader;
  string data;

  this(lua_State L, lua_Reader reader, string data) {
    this.L = L;
    this.reader = reader;
    this.data = data;
  }
}
