import { walk } from "jsr:@std/fs/walk";

let Makefile = "all: build/bison build/libglslang.a\n";

{
  const bisonSrcEntries = await Array.fromAsync(walk("bison", { exts: ["d"] }));
  const bisonSrcPaths = bisonSrcEntries.map(e => e.path).join(" ");

  Makefile += "\n" + `build/bison: ${bisonSrcPaths}\n` +
    "\t" + "dmd -debug -of=$@ $^\n";
}

{
  const glslangSrcEntries = await Array.fromAsync(walk("glslang", { exts: ["d"] }));
  const glslangSrcPaths = glslangSrcEntries.map(e => e.path).join(" ");

  Makefile += "\n" + `build/libglslang.a: ${glslangSrcPaths}\n` +
    "\t" + "dmd -debug -lib -of=$@ $^\n";
}

Deno.writeTextFile("Makefile", Makefile);
