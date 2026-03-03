import { walk } from "jsr:@std/fs/walk";

let Makefile = "all: build/bison build/glslang build/duk\n";

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
  const dukSrcEntries = await Array.fromAsync(walk("duktape", { exts: ["d"] }));
  const dukSrcPaths = dukSrcEntries.map(e => e.path).join(" ");

  Makefile += "\n" + `build/duk: ${dukSrcPaths}\n` +
    "\t" + "dmd -debug -of=$@ $^\n";
}

Deno.writeTextFile("Makefile", Makefile);
