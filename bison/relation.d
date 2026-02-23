module bison.relation;
import bison;

void relation_print_default(size_t i) {
  import std.stdio;
  writef("%3d", i);
}

void relation_print(alias Print, RNode)(string title, RNode[][] r) {
  import std.stdio;
  writef("%s:\n", title);
  foreach (i; 0..r.length)
    if (r[i]) {
      write("    ");
      Print(i);
      write(":");
      for (size_t j = 0; r[i][j] != -1; ++j) {
        write(" ");
        Print(r[i][j]);
      }
      write("\n");
    }
  write("\n");
}

void relation_digraph(RNode)(RNode[][] r, bool[][] function_) {
  size_t infinity = r.length + 2;
  size_t[] indexes = new size_t[r.length + 1];
  size_t[] vertices = new size_t[r.length + 1];
  size_t top = 0;

  bool[][] F = function_;

  void traverse(size_t i) {
    vertices[++top] = i;
    size_t height = indexes[i] = top;

    if (r[i])
      for (size_t j = 0; r[i][j] != -1; ++j) {
        if (indexes[r[i][j]] == 0)
          traverse(r[i][j]);
        
        if (indexes[i] > indexes[r[i][j]])
          indexes[i] = indexes[r[i][j]];

        F[i][] |= F[r[i][j]][];
      }

    if (indexes[i] == height)
      while (true) {
        size_t j = vertices[top--];
        indexes[j] = infinity;

        if (i == j)
          break;

        F[j][] = F[i][];
      }
  }

  foreach (k; 0..r.length)
    if (indexes[k] == 0 && r[k])
      traverse(k);
}

void relation_transpose(RNode)(ref RNode[][] r) {
  if (TRACE_SETS)
    relation_print!relation_print_default("relation_transpose", r);

  size_t[] nedges = new size_t[r.length];
  foreach (i; 0..r.length)
    if (r[i])
      for (size_t j = 0; r[i][j] != -1; ++j)
        ++nedges[r[i][j]];

  RNode[][] new_R = new RNode[][r.length];
  RNode[][] end_R = new RNode[][r.length];
  foreach (i; 0..r.length) {
    RNode[] sp = null;
    if (nedges[i] > 0) {
      sp = new RNode[nedges[i] + 1];
      sp[nedges[i]] = -1;
    }
    new_R[i] = sp;
    end_R[i] = sp;
  }

  foreach (i; 0..r.length)
    if (r[i])
      for (size_t j = 0; r[i][j] != -1; ++j) {
        end_R[r[i][j]][0] = i;
        end_R[r[i][j]] = end_R[r[i][j]][1..$];
      }

  if (TRACE_SETS)
    relation_print!relation_print_default("relation_transpose: output", new_R);

  r = new_R;
}
