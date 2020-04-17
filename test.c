#include "graph.h"

int main() {
  graph_t *topo = build_first_topo();
  dump_nw_graph(topo);
  return 0;
}
