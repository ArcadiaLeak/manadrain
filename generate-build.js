import { walk } from "jsr:@std/fs/walk";

let Makefile = "all: build/bison\n";

{
  const bisonSrcEntries = await Array.fromAsync(walk("bison", { exts: ["d"] }));
  const bisonSrcPaths = bisonSrcEntries.map(e => e.path).join(" ");

  Makefile += "\n" + `build/bison: ${bisonSrcPaths}\n` +
    "\t" + "dmd -debug -of=$@ $^\n";
}

Deno.writeTextFile("Makefile", Makefile);
