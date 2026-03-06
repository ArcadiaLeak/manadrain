import { walk } from "jsr:@std/fs/walk";

let Makefile = "all: build/bison build/glslang build/qjs build/qjs-unit\n";

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
  const qjsSrcEntries = await Array.fromAsync(walk("quickjs", { exts: ["d"] }));
  const qjsSrcPaths = qjsSrcEntries.map(e => e.path).join(" ");

  Makefile += "\n" + `build/qjs: ${qjsSrcPaths}\n` +
    "\t" + "dmd -debug -of=$@ $^\n";
}

{
  const qjsSrcEntries = await Array.fromAsync(walk("quickjs", { exts: ["d"] }));
  const qjsSrcPaths = qjsSrcEntries.map(e => e.path).join(" ");

  Makefile += "\n" + `build/qjs-unit: ${qjsSrcPaths}\n` +
    "\t" + "dmd -debug -unittest -of=$@ $^\n";
}

Deno.writeTextFile("Makefile", Makefile);
