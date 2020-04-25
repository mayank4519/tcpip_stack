#ifndef __LAYER2__
#define __LAYER2__

#include "../net.h"
#include "../glthreads.h"

#define ARP_BROADCAST_REQ 1
#define ARP_REPLY 2
#define ARP_MSG 806
#define MAC_BROADCAST_ADDR 0XFFFFFFFFFFFF

#pragma pack (push,1)
typedef struct arp_hdr_ {

 short hw_type; 	/*1 for ethernet type*/
 short proto_type; 	/*0x0800 for IPV4*/
 char hw_addr_len; 	/*6 for MAC*/
 char proto_addr_len;	/*4 for IPV4*/
 short op_code;  	/*REQUEST OR REPLY*/
 mac_add_t src_mac;    /*MAC OF OIF INTERFACE*/
 unsigned int src_ip;
 mac_add_t dst_mac;
 unsigned int dst_ip;	/*IP for which ARP is being resolved*/
}arp_hdr_t;

typedef struct ethernet_hdr_ {

  mac_add_t src_addr;
  mac_add_t dst_addr;
  unsigned short type;
  char payload[248];
  unsigned int FCS; 
}ethernet_hdr_t;
#pragma pack(pop)

/*static inline bool
l2_frame_recv_qualify_on_interface(interface_t *interface,
                                    ethernet_hdr_t *ethernet_hdr){

  //if(!IS_INTF_L3_MODE(interface))  return false;
  //if(!interface->intf_nw_prop.is_ip_config) return true;


  if(memcmp(ethernet_hdr->dst_addr.mac, 
	   INTF_MAC(interface), 
	   sizeof(mac_add_t)) == 0)
    return true;

  if(IS_MAC_BROADCAST_ADDR(ethernet_hdr->dst_addr.mac))
    return true;

  return false;
}*/

/*ARP table entry*/
typedef struct arp_table_ {
  gl_thread_t arp_entries;
}arp_table_t;

typedef struct arp_entry_ {

  ip_add_t ip_addr;
  mac_add_t mac_addr;
  char oif[16];
  gl_thread_t arp_glue;
}arp_entry_t;
GLTHREAD_TO_STRUCT(arp_glue_to_arp_entry, arp_entry_t, arp_glue);
//#define GET_NODE(curr) (arp_entry_t*)((char*)curr - offsetof(arp_entry_t, arp_glue))

void
init_arp_table(arp_table_t** arp_table);

arp_entry_t *
arp_table_lookup(arp_table_t *arp_table, char *ip_addr);

void
clear_arp_table(arp_table_t *arp_table);

void
delete_arp_table_entry(arp_table_t *arp_table, char *ip_addr);

bool
arp_table_entry_add(arp_table_t *arp_table, arp_entry_t *arp_entry);

void
dump_arp_table(arp_table_t *arp_table);

void
arp_table_update_from_arp_reply(arp_table_t *arp_table,
                                arp_hdr_t *arp_hdr, interface_t *iif);
void
send_arp_broadcast_request(node_t *node,
                           interface_t *oif,
                           char *ip_addr);
static void
process_arp_reply_request(node_t* node, interface_t* iif,
                         ethernet_hdr_t* ethernet_hdr);

static void
process_arp_broadcast_request(node_t* node, interface_t* iif,
                         ethernet_hdr_t* ethernet_hdr);
#endif
