#include "graph.h"
#include<assert.h>
#include<stdio.h>

char*
generate_hash(char* node_name, char* intf_name) {
  return "85:09:51:14:10:11";
}

void
interface_assign_mac_address(interface_t *interface){

    memset(INTF_MAC(interface), 0, 48);
    char* hash_value = generate_hash(interface->attr_node->node_name, interface->if_name);
 
    strcpy(INTF_MAC(interface), hash_value);
    strcat(INTF_MAC(interface), interface->if_name);
}

bool 
node_set_loopback_address(node_t* node, char* ip_addr) {

  assert(ip_addr);

  strncpy(NODE_LO_ADDR(node), ip_addr, 16);
  NODE_LO_ADDR(node)[16] = '\0';
  node->node_nw_prop.is_lb_ip_config = true;

  return true;
}

bool
node_set_intf_ip_addres(node_t* node, char* local_if, char* ip_addr, unsigned int mask) {

  interface_t* intf = get_node_intf_by_name(node, local_if);
  if(intf == NULL) assert(0);

  strncpy(INTF_IP(intf), ip_addr, 16);
  INTF_IP(intf)[16] = '\0'; 
  intf->intf_nw_prop.is_ip_config = true; 
  intf->intf_nw_prop.mask         = mask; 

  return true;
}

void dump_node_nw_prop(node_t* node) {

  printf("Node name: %s\n", node->node_name);
  printf("Node network IP: %s\n", NODE_LO_ADDR(node));
}

void dump_interface_prop(interface_t* intf) {
 
 printf("Interface name: %s\n", intf->if_name);

  if(intf->intf_nw_prop.is_ip_config == true) 
    printf("Interface IP address: %s/%u\n", INTF_IP(intf), intf->intf_nw_prop.mask);

  printf("Inreface MAC address: %s\n", INTF_MAC(intf));
}

void dump_nw_graph(graph_t* topo) {

  interface_t* intf;
  gl_thread_t* curr;
  node_t* node;
  printf("Topology name: %s", topo->topology_name);

  ITERATE_GLTHREAD_BEGIN(&topo->node_list, curr) {
    node = GET_NODE(curr);
    //    node = graph_glue_to_node(curr); //This approach can also be used instead of one written above.
    dump_node_nw_prop(node);
    for(unsigned int i=0; i < MAX_INTF_PER_NODE; i++) {
      intf = node->intf[i];
      if (intf == NULL) break;
      dump_interface_prop(intf);	    
    }
    } ITERATE_GLTHREAD_END(&topo->node_list, curr);
}



