#ifndef __NET__
#define __NET__

#include <memory.h>
#include <stdbool.h>
#include "utils.h"

#define MAX_VLAN_MEMBERSHIP 10

typedef struct node_ node_t;
typedef struct graph_ graph_t;
typedef struct interface_ interface_t;

typedef enum{
  ACCESS,
  TRUNK,
  L2_MODE_UNKNOWN
}intf_l2_mode_t;

static inline char*
intf_l2_mode_str(intf_l2_mode_t l2_mode) {
  switch(l2_mode) {
    case ACCESS:
      return "access";
    case TRUNK:
      return "trunk";
    default:
      return "unknown";
  }
}

#pragma pack (push,1)
typedef struct mac_add_ {
  unsigned char mac[6];
} mac_add_t;

typedef struct ip_add_ {
  unsigned char ip_addr[16];
} ip_add_t;
#pragma (pop)

typedef struct arp_table_ arp_table_t;
typedef struct mac_table_ mac_table_t;
typedef struct rt_table_ rt_table_t;

typedef struct node_nw_prop_ {

  /*L2 properties*/
  arp_table_t* arp_table;
  mac_table_t* mac_table;
  rt_table_t*  rt_table;
  bool is_lb_ip_config;
  ip_add_t lb_ip; /*loopback address of a node*/
}node_nw_prop_t;

extern void
init_arp_table(arp_table_t** arp_table);

extern void
init_mac_table(mac_table_t** mac_table);

extern void
init_rt_table(mac_table_t** mac_table);

static inline void 
init_node_nw_prop(node_nw_prop_t* node_nw_prop) {

  node_nw_prop->is_lb_ip_config = false;
  memset(node_nw_prop->lb_ip.ip_addr, 0, 16);
  init_arp_table(&(node_nw_prop->arp_table));
  init_mac_table(&(node_nw_prop->mac_table));
  init_rt_table(&(node_nw_prop->rt_table));
}

typedef struct intf_nw_prop_ {

  mac_add_t intf_mac; /*mac addr of interface*/
  intf_l2_mode_t intf_l2_mode;
  bool is_ip_config;
  ip_add_t intf_ip;   /*ip address of interface*/
  char mask;
  unsigned int vlans[MAX_VLAN_MEMBERSHIP];
}intf_nw_prop_t;

static inline void
init_intf_nw_prop(intf_nw_prop_t* intf_nw_prop) {

  memset(intf_nw_prop->intf_ip.ip_addr, 0, 16);
  memset(intf_nw_prop->intf_mac.mac, 0, sizeof(intf_nw_prop->intf_mac.mac));
  intf_nw_prop->is_ip_config = false;
  intf_nw_prop->mask = 0;
  intf_nw_prop->intf_l2_mode = L2_MODE_UNKNOWN;
}

void
interface_assign_mac_address(interface_t *interface);

/*GET shorthand Macros*/
#define INTF_MAC(intf_ptr) 	    ((intf_ptr)->intf_nw_prop.intf_mac.mac)
#define INTF_IP(intf_ptr)  	    ((intf_ptr)->intf_nw_prop.intf_ip.ip_addr)

#define NODE_LO_ADDR(node_ptr) 	    ((node_ptr)->node_nw_prop.lb_ip.ip_addr)

#define NODE_ARP_TABLE(node_ptr)    (node_ptr->node_nw_prop.arp_table)
#define NODE_MAC_TABLE(node) 	    (node->node_nw_prop.mac_table)
#define NODE_RT_TABLE(node) 	    (node->node_nw_prop.rt_table)

#define IS_INTF_L3_MODE(intf) 	    (intf->intf_nw_prop.is_ip_config == true)
#define INTF_L2_MODE(intf) 	    (intf->intf_nw_prop.intf_l2_mode)

bool node_set_loopback_address(node_t* node, char* ip_addr);
bool node_set_intf_ip_addres(node_t* node, char* local_if, char* ip_addr, char mask);
void interface_assign_mac_address(interface_t *interface);
void dump_nw_graph(graph_t* graph);

/*ARP related APIs*/
interface_t*
node_get_matching_subnet_interface(node_t* node, char* ip_addr);

#endif
