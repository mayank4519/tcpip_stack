#ifndef __NET__
#define __NET__

#include <memory.h>
#include <stdbool.h>

typedef struct node_ node_t;
typedef struct graph_ graph_t;
typedef struct interface_ interface_t;

typedef struct mac_add_ {
  char mac[48];
} mac_add_t;

typedef struct ip_add_ {
  char ip_addr[16];
} ip_add_t;

typedef struct node_nw_prop_ {

  bool is_lb_ip_config;
  ip_add_t lb_ip; /*loopback address of a node*/
}node_nw_prop_t;

static inline void 
init_node_nw_prop(node_nw_prop_t* node_nw_prop) {
  node_nw_prop->is_lb_ip_config = false;
  memset(node_nw_prop->lb_ip.ip_addr, 0, 16);
}

typedef struct intf_nw_prop_ {

  bool is_ip_config;
  ip_add_t intf_ip;   /*ip address of interface*/
  unsigned int mask;
  mac_add_t intf_mac; /*mac addr of interface*/
}intf_nw_prop_t;

static inline void
init_intf_nw_prop(intf_nw_prop_t* intf_nw_prop) {

  intf_nw_prop->is_ip_config = false;
  memset(intf_nw_prop->intf_ip.ip_addr, 0, 16);
  intf_nw_prop->mask = 0;
  memset(intf_nw_prop->intf_mac.mac, 0, 48);
}

#define INTF_MAC(intf_ptr) ((intf_ptr)->intf_nw_prop.intf_mac.mac)
#define INTF_IP(intf_ptr)  ((intf_ptr)->intf_nw_prop.intf_ip.ip_addr)

#define NODE_LO_ADDR(node_ptr) ((node_ptr)->node_nw_prop.lb_ip.ip_addr)

bool node_set_loopback_address(node_t* node, char* ip_addr);
bool node_set_intf_ip_addres(node_t* node, char* local_if, char* ip_addr, unsigned int mask);
void interface_assign_mac_address(interface_t *interface);
void dump_nw_graph(graph_t* graph);
#endif
