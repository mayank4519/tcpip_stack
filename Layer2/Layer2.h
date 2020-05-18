#ifndef __LAYER2__
#define __LAYER2__

#include "../net.h"
#include "../glthreads.h"

#define ARP_BROADCAST_REQ 1
#define ARP_REPLY 2
#define ARP_MSG 806
#define ETH_IP 0x0800
#define MAC_BROADCAST_ADDR 0XFFFFFFFFFFFF
#define ICMP_PRO        1
#define ETH_IP          0x0800
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

#define VLAN_ETH_FCS(hdr, size)  (*(unsigned int*)((char*)(((vlan_ethernet_hdr_t*)hdr)->payload) + size))

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

#define ETH_FCS(hdr, size)  (*(unsigned int*)((char*)(((ethernet_hdr_t*)hdr)->payload) + size))

/*ARP table entry*/
typedef struct arp_table_ {
  gl_thread_t arp_entries;
}arp_table_t;

typedef struct arp_pending_entry_ arp_pending_entry_t;
typedef struct arp_entry_ arp_entry_t;
typedef void (*arp_processing_fn)(node_t*,
				interface_t*,
				arp_entry_t*,
				arp_pending_entry_t*);

struct arp_pending_entry_{
  gl_thread_t arp_pending_entry_glue;
  arp_processing_fn cb;
  unsigned int pkt_size;
  char pkt[0];
};
GLTHREAD_TO_STRUCT(arp_pending_entry_glue_to_arp_pending_entry, 
			arp_pending_entry_t, arp_pending_entry_glue);

typedef struct arp_entry_ {

  ip_add_t ip_addr;
  mac_add_t mac_addr;
  char oif[16];
  bool is_sane;
  gl_thread_t arp_glue;
  /* List of packets which are pending for
   * this ARP resolution*/
  gl_thread_t arp_pending_list;
}arp_entry_t;

static bool
arp_entry_sane(arp_entry_t *arp_entry) {
  return arp_entry->is_sane;
}

GLTHREAD_TO_STRUCT(arp_glue_to_arp_entry, arp_entry_t, arp_glue);
GLTHREAD_TO_STRUCT(arp_pending_list_to_arp_entry, arp_entry_t, arp_pending_list);

//#define GET_NODE(curr) (arp_entry_t*)((char*)curr - offsetof(arp_entry_t, arp_glue))
//#define GET_NODE(curr) (arp_entry_t*)((char*)curr - offsetof(arp_entry_t, arp_pending_list_glue))

#define IS_ARP_ENTRIES_EQUAL(arp_entry1, arp_entry2)	 \
    (strncmp(arp_entry1->ip_addr.ip_addr, arp_entry2->ip_addr.ip_addr, 16) == 0 &&			      \
     strncmp(arp_entry1->mac_addr.mac, arp_entry2->mac_addr.mac, 6) == 0 && 				      \
     strncmp(arp_entry1->oif, arp_entry2->oif, IF_NAME_SIZE)                    == 0 &&					      \
     arp_entry1->is_sane == arp_entry2->is_sane &&     \
     arp_entry1->is_sane == false)

static inline vlan_802q_hdr_t*
is_pkt_vlan_tagged(ethernet_hdr_t* ethernet_hdr) {

 vlan_802q_hdr_t* vlan_802q_hdr = (vlan_802q_hdr_t*)((char*)ethernet_hdr + 2* sizeof(mac_add_t));

 if (vlan_802q_hdr->tpid == 0X8100)
    return vlan_802q_hdr;

 return NULL;
}

static inline char*
GET_ETHERNET_HDR_PAYLOAD(ethernet_hdr_t* ethernet_hdr) {

  vlan_802q_hdr_t* vlan_802q_hdr = is_pkt_vlan_tagged(ethernet_hdr);
  if(vlan_802q_hdr != NULL) {
    vlan_ethernet_hdr_t* vlan_ethernet_hdr = (vlan_ethernet_hdr_t*)ethernet_hdr;
    return  vlan_ethernet_hdr->payload;
  }
  return ethernet_hdr->payload;
}

static inline unsigned int
GET_ETH_HDR_SIZE_EXCL_PAYLOAD(ethernet_hdr_t *ethernet_hdr){

  vlan_802q_hdr_t* vlan_802q_hdr = is_pkt_vlan_tagged(ethernet_hdr);
  if(vlan_802q_hdr != NULL)
    return VLAN_ETH_HDR_SZ_EXCL_PAYLOAD;

  return ETH_HDR_SZ_EXCL_PAYLOAD;
  
}

static inline void
SET_COMMON_ETH_FCS(ethernet_hdr_t *ethernet_hdr,
                   unsigned int payload_size,
                   unsigned int new_fcs){

    if(is_pkt_vlan_tagged(ethernet_hdr)){
        VLAN_ETH_FCS(ethernet_hdr, payload_size) = new_fcs;
    }
    else{
        ETH_FCS(ethernet_hdr, payload_size) = new_fcs;
    }
}

static inline ethernet_hdr_t*
ALLOC_ETH_HDR_WITH_PAYLOAD(char* pkt, unsigned int pkt_size) {

  char* temp = NULL;
  temp = calloc(1, pkt_size);
  memcpy(temp, pkt, pkt_size);

  ethernet_hdr_t* ethernet_hdr = (ethernet_hdr_t*)(pkt - ETH_HDR_SZ_EXCL_PAYLOAD);
  memset(ethernet_hdr, 0, ETH_HDR_SZ_EXCL_PAYLOAD);
  memcpy(ethernet_hdr->payload, temp, pkt_size);
  SET_COMMON_ETH_FCS(ethernet_hdr, pkt_size, 0);
  free(temp);

  return ethernet_hdr;
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
arp_table_entry_add(arp_table_t *arp_table, arp_entry_t *arp_entry, gl_thread_t **arp_pending_list);

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

/*An API used by ethernet layer to push the packet up towards layer2*/
void
promote_pkt_to_layer2(node_t* node, /*Current node on which pkt is recieved*/
                        interface_t* intf, /*Recieving interface*/
                        ethernet_hdr_t *ethernet_hdr,
                        unsigned int pkt_size); /*payload size*/

/*An API used by layer 5/4/3 to send the packet down to layer 2*/
void
demote_pkt_to_layer2(node_t *node, /*Currenot node*/
		        unsigned int next_hop_ip,  /*If pkt is forwarded to next router, then this is Nexthop IP address (gateway) provid							ed by L3 layer. L2 need to resolve ARP for this IP address*/
		        char *outgoing_intf,       /*The oif obtained from L3 lookup if L3 has decided to forward the pkt. If NULL, then 							L2 will find the appropriate interface*/
		        char *pkt, unsigned int pkt_size,   /*Higher Layers payload*/
		        int protocol_number);

static void
pending_arp_processing_callback_function(node_t* node,
                                interface_t* oif,
                                arp_entry_t* arp_entry,
                                arp_pending_entry_t*);

#endif
