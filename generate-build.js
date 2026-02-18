import { walk } from "jsr:@std/fs/walk";

let Makefile = "all: build/glslang_gen build/glslang\n";

{
  const bisonSrcEntries = await Array.fromAsync(walk("bison", { exts: ["d"] }));
  const bisonSrcPaths = bisonSrcEntries.map(e => e.path).join(" ");

  const glslgramSrcEntries = await Array.fromAsync(walk("glslgram", { exts: ["d"] }));
  const glslgramSrcPaths = glslgramSrcEntries.map(e => e.path).join(" ");

  const genSrcEntries = await Array.fromAsync(walk("glslang_gen", { exts: ["d"] }));
  const genSrcPaths = genSrcEntries.map(e => e.path).join(" ");

  Makefile += "\n" + `build/glslang_gen: ${bisonSrcPaths} ${glslgramSrcPaths} ${genSrcPaths}\n` +
    "\t" + "dmd -debug -of=$@ $^\n";
}

{
  const tabSrcEntries = await Array.fromAsync(walk("glslang_tab", { exts: ["d"] }));
  const tabSrcPaths = tabSrcEntries.map(e => e.path).join(" ");

  const glslangSrcEntries = await Array.fromAsync(walk("glslang", { exts: ["d"] }));
  const glslangSrcPaths = glslangSrcEntries.map(e => e.path).join(" ");

  Makefile += "\n" + `build/glslang: ${glslangSrcPaths} ${tabSrcPaths}\n` +
    "\t" + "dmd -debug -of=$@ $^\n";
}

Deno.writeTextFile("Makefile", Makefile);
