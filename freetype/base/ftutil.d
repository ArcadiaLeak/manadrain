module freetype.base.ftutil;

import freetype;

void ft_mem_free(
  FT_Memory memory, void* P
) { if (P) memory.free(memory, P); }
