#include "graph.h"
#include "utils.h"
#include<assert.h>
#include<stdio.h>

char*
generate_hash(char* node_name, char* intf_name) {
  return "850951141011";
}

void
interface_assign_mac_address(interface_t *interface){

    memset(INTF_MAC(interface), 0, 6);
    char* hash_value = generate_hash(interface->attr_node->node_name, interface->if_name);
 
    strcpy(INTF_MAC(interface), hash_value);
    strcat(INTF_MAC(interface), interface->if_name);
}

bool 
node_set_loopback_address(node_t* node, char* ip_addr) {

  assert(ip_addr);

  strncpy(NODE_LO_ADDR(node), ip_addr, 16);
  NODE_LO_ADDR(node)[15] = '\0';
  node->node_nw_prop.is_lb_ip_config = true;

  return true;
}

bool
node_set_intf_ip_addres(node_t* node, char* local_if, char* ip_addr, char mask) {

  interface_t* intf = get_node_intf_by_name(node, local_if);
  if(intf == NULL) assert(0);

  strncpy(INTF_IP(intf), ip_addr, 16);
  INTF_IP(intf)[15] = '\0'; 
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
    printf("Interface IP address: %s/%c\n", INTF_IP(intf), intf->intf_nw_prop.mask);

  printf("Inreface MAC address: %u:%u:%u:%u:%u:%u\n", 
	INTF_MAC(intf)[0], INTF_MAC(intf)[1], 
	INTF_MAC(intf)[2], INTF_MAC(intf)[3], 
	INTF_MAC(intf)[4], INTF_MAC(intf)[5]);
}

void dump_nw_graph(graph_t* topo) {

  interface_t* intf;
  gl_thread_t* curr;
  node_t* node;
  printf("Topology name: %s", topo->topology_name);

  ITERATE_GLTHREAD_BEGIN(&topo->node_list, curr) {
    //node = GET_NODE(curr); //This approach can also be used instead of one written above.
    node = graph_glue_to_node(curr);
    dump_node_nw_prop(node);
    for(unsigned int i=0; i < MAX_INTF_PER_NODE; i++) {
      intf = node->intf[i];
      if (intf == NULL) break;
      dump_interface_prop(intf);	    
    }
  } ITERATE_GLTHREAD_END(&topo->node_list, curr);
}

interface_t*
node_get_matching_subnet_interface(node_t* node, char* ip_addr) {

  int i=0;
  interface_t* intf = NULL;
  char* intf_ip = NULL;
  char intf_mask;

  char subnet1[16], subnet2[16];

  for( ; i < MAX_INTF_PER_NODE; i++) {
    intf = node->intf[i];
    if(intf == NULL)
	return NULL;
    
    if(intf->intf_nw_prop.is_ip_config == false)
	continue;

    intf_ip = INTF_IP(intf);
   intf_mask = intf->intf_nw_prop.mask;

   memset(subnet1, 0, 16);
   memset(subnet2, 0, 16);

   apply_mask(intf_ip, intf_mask, subnet1); 
   apply_mask(ip_addr, intf_mask, subnet2); 

   if(strncmp(subnet1, subnet2, 16) == 0) {
     return intf;
   }
  }

}


