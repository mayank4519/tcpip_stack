#include "graph.h"
#include "CommandParser/libcli.h"

graph_t *topo = NULL;

extern graph_t *build_first_topo();
extern graph_t *build_simple_l2_switch_topo();
extern graph_t *build_dualswitch_topo();
extern graph_t *linear_3_node_topo();
extern graph_t* build_square_topo();
extern void nw_init_cli();

int
main() {

  nw_init_cli();

  topo   = build_square_topo();
  start_shell();
  return 0;
}
