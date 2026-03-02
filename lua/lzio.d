module lua.lzio;
import lua;

struct ZIO {
  size_t n;
  size_t p;
  lua_Reader reader;
  string data;
  lua_State L;
}
