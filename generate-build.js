import { walk } from "jsr:@std/fs/walk";

let Makefile = "all: build/bison build/glslang build/lua\n";

{
  const bisonSrcEntries = await Array.fromAsync(walk("bison", { exts: ["d"] }));
  const bisonSrcPaths = bisonSrcEntries.map(e => e.path).join(" ");

  Makefile += "\n" + `build/bison: ${bisonSrcPaths}\n` +
    "\t" + "dmd -debug -of=$@ $^\n";
}

{
  const glslangSrcEntries = await Array.fromAsync(walk("glslang", { exts: ["d"] }));
  const glslangSrcPaths = glslangSrcEntries.map(e => e.path).join(" ");

  Makefile += "\n" + `build/glslang: ${glslangSrcPaths}\n` +
    "\t" + "dmd -debug -of=$@ $^\n";
}

{
  const luaSrcEntries = await Array.fromAsync(walk("lua", { exts: ["d"] }));
  const luaSrcPaths = luaSrcEntries.map(e => e.path).join(" ");

  Makefile += "\n" + `build/lua: ${luaSrcPaths}\n` +
    "\t" + "dmd -debug -of=$@ $^\n";
}

Deno.writeTextFile("Makefile", Makefile);
