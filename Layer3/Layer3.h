#ifndef __LAYER3__
#define __LAYER3__

#include "../glthreads.h"

typedef struct l3_route_ {
  
  char dest[16];  /*key*/
  char mask;      /*key*/
  bool is_direct; /*If set to true, then gw_ip and oif has no meaning*/
  char gw_ip[16]; /*Next hop IP*/
  char oif[IF_NAME_SIZE];/*OIF*/
  gl_thread_t rt_glue;
}l3_route_t;

typedef struct rt_table_ {
  gl_thread_t route_list;
}rt_table_t;
GLTHREAD_TO_STRUCT(rt_glue_to_l3_route, l3_route_t, rt_glue);

#define IS_ROUTE_EQUAL(route1, route2) 				\
	((strncmp(route1->dest, route2->dest, 16) == 0) &&   	\
	(route1->mask == route2->mask) && 		  	\
	(route1->is_direct == route2->is_direct) &&		\
	(strncmp(route1->gw_ip, route2->gw_ip, 16) == 0) && 	\
	(strncmp(route1->oif, route2->oif, 16) == 0))

#endif

#pragma pack (push,1)

/*The Ip hdr format as per the standard specification*/
typedef struct ip_hdr_{

    unsigned int version : 4 ;  /*version number, always 4 for IPv4 protocol*/
    unsigned int ihl : 4 ;      /*length of IP hdr, in 32-bit words unit. for Ex, if this value is 5, it means length of this ip hdr is 20Bytes*/
    char tos;
    unsigned short total_length;         /*length of hdr + ip_hdr payload*/

    /* Fragmentation Related members, we shall not be using below members
     * as we will not be writing fragmentation code. if you wish, take it
     * as a extension of the project */
    unsigned short identification;
    unsigned int unused_flag : 1 ;
    unsigned int DF_flag : 1;
    unsigned int MORE_flag : 1;
    unsigned int frag_offset : 13;


    char ttl;
    char protocol;
    short checksum;
    unsigned int src_ip;
    unsigned int dst_ip;
} ip_hdr_t;

#pragma pack(pop)

static inline void
initialize_ip_hdr(ip_hdr_t *ip_hdr){

    ip_hdr->version = 4;
    ip_hdr->ihl = 5; /*We will not be using option field, hence hdr size shall always be 5*4 = 20B*/
    ip_hdr->tos = 0;

    ip_hdr->total_length = 0; /*To be filled by the caller*/

    /*Fragmentation related will not be used
 *      * int this course, initialize them all to zero*/
    ip_hdr->identification = 0;
    ip_hdr->unused_flag = 0;
    ip_hdr->DF_flag = 1;
    ip_hdr->MORE_flag = 0;
    ip_hdr->frag_offset = 0;

    ip_hdr->ttl = 64; /*Let us use 64*/
    ip_hdr->protocol = 0; /*To be filled by the caller*/
    ip_hdr->checksum = 0; /*Not used in this course*/
    ip_hdr->src_ip = 0; /*To be filled by the caller*/
    ip_hdr->dst_ip = 0; /*To be filled by the caller*/
}

#define IP_HDR_LEN_IN_BYTES(ip_hdr_ptr) (ip_hdr_ptr->ihl * 4)
#define IP_HDR_TOTAL_LEN_IN_BYTES(ip_hdr_ptr) (ip_hdr_ptr->total_length * 4)
#define INCREMENT_IPHDR(ip_hdr_ptr) ((char*)ip_hdr_ptr + (ip_hdr_ptr->ihl * 4))
#define IP_HDR_PAYLOAD_SIZE(ip_hdr_ptr) ((IP_HDR_TOTAL_LEN_IN_BYTES(ip_hdr_ptr) - \
						IP_HDR_LEN_IN_BYTES(ip_hdr_ptr))

/*An API used by ethernet layer to push the packet up towards layer3*/
void
promote_pkt_to_layer3(node_t* node, /*Current node on which pkt is recieved*/
			interface_t* intf, /*Recieving interface*/
			char* pkt,         /*L3 Payload*/
			unsigned int pkt_size, /*L3 payload size*/
			int L3_protocol_number);/*Obtained from eth_hdr->type field*/

/*An API used by layer 4 or layer 5 to send the packet down to layer 3*/
void
demote_pkt_to_layer3(node_t* node, /*Current node on which pkt is recieved*/
			char *pkt, unsigned int size,
			int protocol_number, /*L4 or L5 protocol type*/
			unsigned int dest_ip_address);
