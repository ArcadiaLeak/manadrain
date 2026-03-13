import { walk } from "jsr:@std/fs/walk";

let Makefile = "all: build/bison build/glslang build/qjs\n";

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
  const qjsSrcEntries = await Array.fromAsync(walk("quickjs", { exts: ["cpp"] }));
  const qjsSrcPaths = qjsSrcEntries.map(e => e.path).join(" ");

  Makefile += "\n" + `build/qjs: ${qjsSrcPaths}\n` +
    "\t" + "g++ -std=c++26 -Wconversion -Wimplicit-fallthrough -O0 -g -o $@ $^\n";
}

Deno.writeTextFile("Makefile", Makefile);
