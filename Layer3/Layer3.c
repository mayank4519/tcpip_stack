#include <stdio.h>
#include "graph.h"
#include "Layer3.h"
#include <sys/socket.h>
#include <memory.h>
#include <stdlib.h>
#include "comm.h"
#include "tcpconst.h"
#include <arpa/inet.h> /*for inet_ntop & inet_pton*/
#include <assert.h>

init_rt_table(rt_table_t** rt_table) {
  *rt_table = calloc(1, sizeof(rt_table_t));
  glthread_node_init(&((*rt_table)->route_list)); 
}

l3_route_t *
rt_table_lookup(rt_table_t *rt_table, char *ip_addr, char mask){

  gl_thread_t* curr;
  l3_route_t* l3_route;

  ITERATE_GLTHREAD_BEGIN(&rt_table->route_list, curr) {

   l3_route = rt_glue_to_l3_route(curr);
   if(l3_route == NULL)
     return NULL;

   if(strncmp(l3_route->dest, ip_addr, 16) == 0 &&
	l3_route->mask == mask)
     return l3_route;
    
  }ITERATE_GLTHREAD_END(&rt_table->route_list, curr);

}

/*Look up L3 routing table using longest prefix match*/
l3_route_t *
l3rib_lookup_lpm(rt_table_t *rt_table,
                 char* dest_ip){

  gl_thread_t* curr;
  l3_route_t* l3_route = NULL, *default_route = NULL, *lpm_route = NULL;
  char subnet[16];
  char longest_mask = 0;
  ITERATE_GLTHREAD_BEGIN(&rt_table->route_list, curr) {
    
    l3_route = rt_glue_to_l3_route(curr);

    memset(subnet, 0, 16);

    apply_mask(dest_ip, l3_route->mask, subnet);

    if(strncmp("0.0.0.0", l3_route->dest, 16) == 0 && 
		l3_route->mask == 0) {
      default_route = l3_route;
    }

    else if(strncmp(subnet, l3_route->dest, strlen(subnet)) == 0) {
      if(l3_route->mask > longest_mask) {
	longest_mask = l3_route->mask;
        lpm_route = l3_route;
      }
    }

  }ITERATE_GLTHREAD_END(&rt_table->route_list, curr); 

  return lpm_route ? lpm_route : default_route;

}

void
delete_rt_table_entry(rt_table_t* rt_table, char* ip, char mask) {

  char nw_ip[16] = {0};
  apply_mask(ip, mask, nw_ip);

  l3_route_t* dl_route = rt_table_lookup(rt_table, nw_ip, mask);
  if(dl_route == NULL)
    return;

  remove_glthread(&dl_route->rt_glue);
  free(dl_route);
}

bool
_rt_table_entry_add(rt_table_t *rt_table,
			l3_route_t* l3_route) {

 l3_route_t* old_route = rt_table_lookup(rt_table, l3_route->dest, l3_route->mask);
 if(old_route && 
	IS_ROUTE_EQUAL(l3_route, old_route))
   return false;

 if(old_route)
   delete_rt_table_entry(rt_table, old_route->dest, old_route->mask);

 glthread_node_init(&l3_route->rt_glue);
 glthread_add_next(&rt_table->route_list, &l3_route->rt_glue);

 return true; 
}

void
rt_table_add_route(rt_table_t *rt_table,
                   char *dst, char mask,
                   char *gw, char *oif) {

 l3_route_t* l3_route;
 char nw_ip[16] = {0};

 apply_mask(dst, mask, nw_ip);

 l3_route = l3rib_lookup_lpm(rt_table, dst);

 assert(l3_route == NULL);

 l3_route = calloc(1, sizeof(l3_route_t));

 strncpy(l3_route->dest, nw_ip, strlen(nw_ip));
 l3_route->dest[15] = '\0';
 l3_route->mask  = mask;
 if(!gw && !oif)
   l3_route->is_direct = true;
 else
   l3_route->is_direct = false;

 if(gw && oif) {
   strncpy(l3_route->gw_ip, gw, 16);
   l3_route->gw_ip[15] = '\0';
   strncpy(l3_route->oif, oif, IF_NAME_SIZE);
   l3_route->oif[IF_NAME_SIZE - 1] = '\0';
 }

 if(_rt_table_entry_add(rt_table, l3_route) == false) {
   printf("Route addition failed for IP=%s and mask=%u\n",
	   nw_ip, mask);
   free(l3_route);
 }

}

void
rt_table_add_direct_route(rt_table_t *rt_table,
                          char *dst, char mask) {

  rt_table_add_route(rt_table, dst, mask, 0, 0);
}

void
dump_rt_table(rt_table_t *rt_table) {
  
  printf("Routing table\n");
  printf("Dest IP: \t | Mask\t | is_Direct\t | IP\t | Interface\n");
  gl_thread_t* curr;
  l3_route_t* l3_route;

  ITERATE_GLTHREAD_BEGIN(&rt_table->route_list, curr) {

   l3_route = rt_glue_to_l3_route(curr);
   if(l3_route == NULL)
     return NULL;

  printf("\t%-18s %-4d %-4d %-18s %s\n", 
		l3_route->dest, l3_route->mask,
		l3_route->is_direct, l3_route->gw_ip,
		l3_route->oif);

  }ITERATE_GLTHREAD_END(&rt_table->route_list, curr);

}

bool
l3_is_direct_route(l3_route_t *l3_route) {

  return l3_route->is_direct;
}

bool
is_layer3_local_delivery(node_t *node, unsigned int dst_ip) {
  
  char ip[16] = {};
  unsigned int i = 0;
  interface_t* intf = NULL;

  inet_ntop(AF_INET, &dst_ip, ip, 16);

  if(strncmp(NODE_LO_ADDR(node), ip, 16) == 0)
    return true;

  for(; i < MAX_INTF_PER_NODE; i++) {
    intf = node->intf[i];

    if(intf == NULL) return false;

    if(intf->intf_nw_prop.is_ip_config == false)
      continue;
 
    if(strncmp(INTF_IP(intf), ip, 16) == 0)
      return true;

  }  
  return false;
}

extern void
demote_pkt_to_layer2(node_t *node, /*Currenot node*/
                        unsigned int next_hop_ip,  /*If pkt is forwarded to next router, then this is Nexthop IP address (gateway) provid                                                       ed by L3 layer. L2 need to resolve ARP for this IP address*/
                        char *outgoing_intf,       /*The oif obtained from L3 lookup if L3 has decided to forward the pkt. If NULL, then                                                        L2 will find the appropriate interface*/
                        char *pkt, unsigned int pkt_size,   /*Higher Layers payload*/
                        int protocol_number);

static void
layer3_ip_pkt_recv_from_bottom(node_t *node, interface_t *interface,
				ip_hdr_t* ip, unsigned int pkt_size) {

  printf("LAYER3: Packet recieved from layer 2\n");
  ip_hdr_t* ip_hdr = (ip_hdr_t*)ip;
  char dst_ip[16];
  
  inet_ntop(AF_INET, &(ip_hdr->dst_ip), dst_ip, 16);
 
  l3_route_t* l3_route = l3rib_lookup_lpm(NODE_RT_TABLE(node), dst_ip); 

  if(l3_route == NULL) {
    printf("Node: %s, No route present for IP: %s\n", node->node_name, dst_ip);
    return;
  }
  else
    printf("Node: %s, There is a route present for IP: %s\n", node->node_name, dst_ip); 

  if(l3_is_direct_route(l3_route)) {

    if(is_layer3_local_delivery(node, ip_hdr->dst_ip)) {
      switch(ip_hdr->protocol) {
        case ICMP_PRO:
	  printf("Ping sucess for IP address: %s\n", dst_ip);
	  break;
        default:
	  ;
      }
      return;
    }

    demote_pkt_to_layer2(node, 
			0, NULL, 
			(char*)ip_hdr, pkt_size, 
			ETH_IP); 
  }
 
  ip_hdr->ttl--;

  if(ip_hdr->ttl == 0) {
    printf("TTL: %u, Drop the packet!\n", ip_hdr->ttl);
    return;
  }
 
  unsigned int next_hop_ip;
  inet_pton(AF_INET, l3_route->gw_ip, &next_hop_ip);
  next_hop_ip = htonl(next_hop_ip);

  demote_pkt_to_layer2(node, 
		next_hop_ip, l3_route->oif, 
		(char*)ip_hdr, pkt_size, 
		ETH_IP);

}

void
layer3_pkt_recv_from_bottom(node_t *node, interface_t *interface,
                            char *pkt, unsigned int pkt_size,
                            int L3_protocol_type) {

  switch(L3_protocol_type) {
    case ETH_IP:
    case IP_IN_IP:
      layer3_ip_pkt_recv_from_bottom(node, interface, (ip_hdr_t*)pkt, pkt_size);
      break;
    default:
	;
  }

}

void
promote_pkt_to_layer3(node_t* node, /*Current node on which pkt is recieved*/
                        interface_t* intf, /*Recieving interface*/
                        char* pkt,         /*L3 Payload*/
                        unsigned int pkt_size, /*L3 payload size*/
                        int L3_protocol_number) {

  layer3_pkt_recv_from_bottom(node, intf,pkt, pkt_size, L3_protocol_number);

}

static void
layer3_pkt_receieve_from_top(node_t* node,
		char *pkt, unsigned int size,
		int protocol_number, unsigned int dest_ip_address) {

  printf("LAYER3: Packet recieved on layer 3 from upper layer\n");

  ip_hdr_t ip_hdr;  
  initialize_ip_hdr(&ip_hdr);
  char dest_ip[16] = {};
  dest_ip_address = ntohl(dest_ip_address);

  ip_hdr.total_length = (unsigned short)ip_hdr.ihl + 
			(unsigned short)size / 4 + 
			(unsigned short)((size % 4) ? 1 : 0);

  ip_hdr.protocol = protocol_number;
  ip_hdr.dst_ip = dest_ip_address;

  unsigned int addr_int;
  inet_pton(AF_INET, NODE_LO_ADDR(node), &addr_int);
  addr_int = htonl(addr_int);//MAYANK: No need of this i think.

  ip_hdr.src_ip = addr_int;

  inet_ntop(AF_INET, &dest_ip_address, dest_ip, 16);
  l3_route_t *l3_route = l3rib_lookup_lpm(NODE_RT_TABLE(node), dest_ip);

  if(l3_route == NULL) {
    printf("Node: %s, No route present for IP: %s\n", node->node_name, dest_ip);
    return;
  }

  unsigned int next_hop_ip;
  bool is_direct_route = l3_is_direct_route(l3_route);
  /*Resolve next hop*/
  if(is_direct_route) {
    next_hop_ip = dest_ip_address;
   next_hop_ip = htonl(next_hop_ip);
  }
  else {
    inet_pton(AF_INET, l3_route->gw_ip, &next_hop_ip);
    next_hop_ip = htonl(next_hop_ip);
  }

  char* new_pkt = NULL;
  unsigned int new_pkt_size = 0;

  new_pkt = calloc(1, MAX_PACKET_BUFFER_SIZE);
  new_pkt_size = ip_hdr.total_length * 4;

  memset(new_pkt, 0, MAX_PACKET_BUFFER_SIZE);
  memcpy(new_pkt, (char*)&ip_hdr, ip_hdr.ihl * 4);

  if(pkt && size)
    memcpy(new_pkt + (ip_hdr.ihl * 4), pkt, size);

  char* right_shift_ptr = pkt_buffet_right_shift(new_pkt, 
				new_pkt_size, MAX_PACKET_BUFFER_SIZE);

  demote_pkt_to_layer2(node, 
			next_hop_ip, is_direct_route ? 0 : l3_route->oif, 
			right_shift_ptr, new_pkt_size, 
			ETH_IP);

  free(new_pkt);
}

void
demote_pkt_to_layer3(node_t* node, /*Current node on which pkt is recieved*/
                        char *pkt, unsigned int size,
                        int protocol_number, /*L4 or L5 protocol type*/
                        unsigned int dest_ip_address) {

  layer3_pkt_receieve_from_top(node, pkt, size, 
				protocol_number, dest_ip_address);
}
