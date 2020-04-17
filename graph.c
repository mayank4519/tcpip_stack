#include "graph.h"
#include <string.h>
#include <stdlib.h>

graph_t* create_new_graph(char* topology_name) {
  graph_t* graph = calloc(1, sizeof(graph_t));
  strncpy(graph->topology_name, topology_name, 32);
  graph->topology_name[32] = '\0';

  glthread_node_init(&graph->node_list);
  return graph;
}

node_t* create_graph_node(graph_t* graph, char* node_name) {
  node_t* node = calloc(1, sizeof(node_t));
  strncpy(node->node_name, node_name, NODE_NAME_SIZE);
  node->node_name[NODE_NAME_SIZE] = '\0';

  glthread_node_init(&node->graph_glue);
  glthread_add_next(&graph->node_list, &node->graph_glue);

  return node;
}

void 
insert_link_between_two_nodes(node_t* n1, 
	node_t* n2, 
	char* from_intf, 
	char* to_intf, 
	unsigned int cost) {

 link_t* link = calloc(1, sizeof(link_t));

 strncpy(link->intf1.if_name, from_intf, IF_NAME_SIZE);
 link->intf1.if_name[IF_NAME_SIZE] = '\0';
 strncpy(link->intf2.if_name, to_intf, IF_NAME_SIZE); 
 link->intf2.if_name[IF_NAME_SIZE] = '\0';

 link->intf1.link = link;/*set back pointer to link*/
 link->intf2.link = link;/*set back pointer to link*/

 link->intf1.attr_node = n1;
 link->intf2.attr_node = n2;
 link->cost = cost;

 int empty_intf_slot;

 empty_intf_slot = find_intf_available_slot(n1);
 n1->intf[empty_intf_slot] = &link->intf1; 

 empty_intf_slot = find_intf_available_slot(n2);
 n2->intf[empty_intf_slot] = &link->intf2; 

 init_node_nw_prop(&link->intf1.intf_nw_prop);
 init_node_nw_prop(&link->intf2.intf_nw_prop);

 /*Assign random MAC*/
 interface_assign_mac_address(&link->intf1); 
 interface_assign_mac_address(&link->intf2); 
}

void dump_graph(graph_t* topo) {

  gl_thread_t *curr;
  node_t *node;
  printf("Topology name: %s\n", topo->topology_name);
  ITERATE_GLTHREAD_BEGIN(&topo->node_list, curr) {
    //node = GET_NODE(curr);
    node = graph_glue_to_node(curr);
    dump_node(node);
  } ITERATE_GLTHREAD_END(&topo->node_list, curr);
}

void dump_node(node_t* node) {

  interface_t* intf;

  printf("Node name: %s\n", node->node_name);
  for(int i=0; i < MAX_INTF_PER_NODE; i++) {
    intf = node->intf[i];
    if(intf == NULL)
        break;
    dump_intf(intf);
  }
}

void dump_intf(interface_t* intf) {

  link_t *link = intf->link;
  printf("Local node: %s, Interface name: %s, link cost: %u\n",
        intf->attr_node->node_name,
        intf->if_name,
        link->cost);
}

