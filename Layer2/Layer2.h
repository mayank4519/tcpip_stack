#ifndef __LAYER2__
#define __LAYER2__

#include "../net.h"
#include "../glthreads.h"

#define ARP_BROADCAST_REQ 1
#define ARP_REPLY 2
#define ARP_MSG 806
#define MAC_BROADCAST_ADDR 0XFFFFFFFFFFFF

/*VLAN Support*/

#pragma pack (push,1)
typedef struct vlan_802q_hdr_ {

  unsigned short tpid; /*0X8100*/
  short tci_pcp : 3;   /*intial 4 bits are not used*/
  short tci_dei : 1;   /*Not used*/
  short tci_vid : 12;  /*VLAN ranges*/
}vlan_802q_hdr_t;

typedef struct vlan_ethernet_hdr_ {

  mac_add_t dst_addr;
  mac_add_t src_addr;
  vlan_802q_hdr_t vlan_802q_hdr;
  short type;
  char payload[248];
  unsigned int FCS;
}vlan_ethernet_hdr_t;
#pragma pack (pop)

#define VLAN_ETH_HDR_SZ_EXCL_PAYLOAD \
		(sizeof(vlan_ethernet_hdr_t) - sizeof(((vlan_ethernet_hdr_t*)0)->payload))

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

  mac_add_t dst_addr;
  mac_add_t src_addr;
  short type;
  char payload[248];
  unsigned int FCS; 
}ethernet_hdr_t;
#pragma pack(pop)

#define ETH_HDR_SZ_EXCL_PAYLOAD \
		(sizeof(ethernet_hdr_t) - sizeof(((ethernet_hdr_t*)0)->payload))

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

static inline vlan_802q_hdr_t*
is_pkt_vlan_tagged(ethernet_hdr_t* ethernet_hdr) {

 vlan_802q_hdr_t* vlan_802q_hdr = (vlan_802q_hdr_t*)((char*)ethernet_hdr + 2* sizeof(mac_add_t));

 if (vlan_802q_hdr->tpid == 0X8100)
    return vlan_802q_hdr;

 return NULL;
}

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

void
node_set_intf_l2_mode(node_t* node, char* iif,
			intf_l2_mode_t l2_mode);


ethernet_hdr_t*
tag_pkt_with_vlan_id(ethernet_hdr_t* ethernet_hdr,
			unsigned int total_pkt_size,
			int vlan_id,
                        unsigned int *new_pkt_size);

ethernet_hdr_t*
untag_pkt_with_vlan_id(ethernet_hdr_t* ethernet_hdr,
			unsigned int total_pkt_size,
			unsigned int *new_pkt_size);

#endif
