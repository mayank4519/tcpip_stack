#include "graph.h"
#include "utils.h"
#include<assert.h>
#include<stdio.h>

/*Just some Random number generator*/
static unsigned int
hash_code(void *ptr, unsigned int size){
    unsigned int value=0, i =0;
    char *str = (char*)ptr;
    while(i < size)
    {
        value += *str;
        value*=97;
        str++;
        i++;
    }
    return value;
}

void
interface_assign_mac_address(interface_t *interface){

    unsigned int hash_code_val = hash_code(interface, sizeof(interface_t));
    memset(INTF_MAC(interface), 0, sizeof(mac_add_t));
    memcpy(INTF_MAC(interface), (char *)&hash_code_val, sizeof(unsigned int)); 
}

extern void
rt_table_add_direct_route(rt_table_t *rt_table,
                          char *dst, char mask);

bool 
node_set_loopback_address(node_t* node, char* ip_addr) {

  assert(ip_addr);

  strncpy(NODE_LO_ADDR(node), ip_addr, 16);
  NODE_LO_ADDR(node)[15] = '\0';
  node->node_nw_prop.is_lb_ip_config = true;
  rt_table_add_direct_route(NODE_RT_TABLE(node), ip_addr, 32);
  return true;
}

bool
node_set_intf_ip_address(node_t* node, char* local_if, char* ip_addr, char mask) {

  interface_t* intf = get_node_intf_by_name(node, local_if);
  if(intf == NULL) assert(0);

  strncpy(INTF_IP(intf), ip_addr, 16);
  INTF_IP(intf)[15] = '\0'; 
  intf->intf_nw_prop.is_ip_config = true; 
  intf->intf_nw_prop.mask         = mask; 
  rt_table_add_direct_route(NODE_RT_TABLE(node), ip_addr, mask);

  return true;
}

void dump_node_nw_prop(node_t* node) {

  printf("Node name= %s\t", node->node_name);
  printf("udp_sock_fd: %u\n", node->udp_sock_fd);
  if(node->node_nw_prop.is_lb_ip_config == true)
    printf("Node network IP= %s\n", NODE_LO_ADDR(node));
}

void dump_interface_prop(interface_t* intf) {
 
 unsigned int i = 0;

 printf("\nInterface name= %s\n", intf->if_name);
 printf("l2 mode= %s\t", intf_l2_mode_str(INTF_L2_MODE(intf)));

  if(intf->intf_nw_prop.is_ip_config == true) 
    printf("Interface IP address: %s/%c\n", INTF_IP(intf), intf->intf_nw_prop.mask);

  printf("Interface MAC address: %u:%u:%u:%u:%u:%u\n", 
	INTF_MAC(intf)[0], INTF_MAC(intf)[1], 
	INTF_MAC(intf)[2], INTF_MAC(intf)[3], 
	INTF_MAC(intf)[4], INTF_MAC(intf)[5]);

  printf("vlan membership : ");
  for ( ; i < MAX_VLAN_MEMBERSHIP; i++) {
    if(intf->intf_nw_prop.vlans[i] == 0) {
      printf("\n\n");
      return;
    }

    printf("%u ", intf->intf_nw_prop.vlans[i]);
  }
  printf("\n");
}

void dump_nw_graph(graph_t* topo) {

  interface_t* intf;
  gl_thread_t* curr;
  node_t* node;
  printf("Topology name: %s\n", topo->topology_name);

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

bool
is_trunk_interface_vlan_enabled(interface_t* intf, unsigned int vid) {

  unsigned int i = 0;

  if (INTF_L2_MODE(intf) != TRUNK)
    assert(0);

  for(; i < MAX_VLAN_MEMBERSHIP; i++) {
    if(intf->intf_nw_prop.vlans[i] == vid)
      return true;
  }
  return false;
}


/*When pkt moves from top to down in TCP/IP stack, we would need
  room in the pkt buffer to attach more new headers. Below function
  simply shifts the pkt content present in the start of the pkt buffer
  towards right so that new room is created*/

char*
pkt_buffet_right_shift(char* pkt,
			unsigned int pkt_size,
			unsigned int total_buffer_size) {

  char* temp = NULL;
  bool need_temp_memory = false;

  if(pkt_size * 2 > total_buffer_size)
    need_temp_memory = true;

  if(need_temp_memory) {
    temp = calloc(1, pkt_size);
    memset(temp, 0, pkt_size);
    memcpy(temp, pkt, pkt_size);

    memset(pkt, 0, total_buffer_size);
    memcpy(pkt + (total_buffer_size - pkt_size), temp, pkt_size);
    free(temp);
    return pkt + (total_buffer_size - pkt_size);
  }

  memcpy(pkt + (total_buffer_size - pkt_size), pkt, pkt_size);
  memset(pkt, 0, pkt_size);
  return (pkt + (total_buffer_size - pkt_size));

}

