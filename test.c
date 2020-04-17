#include "graph.h"
#include "CommandParser/libcli.h"

graph_t *topo = NULL;

extern graph_t *build_first_topo();
extern void nw_init_cli();

int
main() {

  nw_init_cli();

  topo = build_first_topo();

  start_shell();
  return 0;
}
