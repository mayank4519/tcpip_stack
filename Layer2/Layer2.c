#include "Layer2.h"
#include <stdlib.h>
#include <sys/socket.h>
#include "../graph.h"
#include <stdio.h>
#include <assert.h>

void
init_arp_table(arp_table_t** arp_table) {

  *arp_table = calloc(1, sizeof(arp_table_t));
  glthread_node_init(&((*arp_table)->arp_entries)); 
}

static bool
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
}


void
layer2_frame_recv(node_t *node, interface_t *interface,
                     char *pkt, unsigned int pkt_size){

  ethernet_hdr_t *ethernet_hdr = (ethernet_hdr_t *)pkt;

    if(l2_frame_recv_qualify_on_interface(interface, ethernet_hdr) == false){

        printf("L2 Frame Rejected");
        return;
    }

    printf("L2 Frame Accepted\n");
    switch(ethernet_hdr->type) {
      case ARP_MSG:
      {
	arp_hdr_t* arp_hdr = (arp_hdr_t*)ethernet_hdr->payload;	
	switch(arp_hdr->op_code) {
	  case ARP_REPLY:
	    process_arp_reply_request(node, interface, ethernet_hdr);
	    break;
	  case ARP_BROADCAST_REQ:
	    process_arp_broadcast_request(node, interface, ethernet_hdr);
	    break;
	  default:
	    break;
	}
      }
      break;
      default:
	printf("Not an ARP msg\n");
	break;
    }
    
}

arp_entry_t *
arp_table_lookup(arp_table_t *arp_table, char *ip_addr) {
 
 arp_entry_t* arp_entry;
 gl_thread_t* curr;

 ITERATE_GLTHREAD_BEGIN(&arp_table->arp_entries, curr) {
   arp_entry = arp_glue_to_arp_entry(curr);
   if(strncmp(arp_entry->ip_addr.ip_addr, ip_addr, 16) == 0)
	return arp_entry; 
 }ITERATE_GLTHREAD_END(&arp_table->arp_entries, curr); 
 return NULL;

}

void
delete_arp_table_entry(arp_table_t *arp_table, char *ip_addr) {

 arp_entry_t* arp_entry = arp_table_lookup(arp_table, ip_addr);
 
 if(arp_entry == NULL)
   return;

 remove_glthread(&arp_entry->arp_glue); 
 free(arp_entry); 

}

bool
arp_table_entry_add(arp_table_t *arp_table, arp_entry_t *arp_entry) {

  arp_entry_t* old_arp_entry = arp_table_lookup(arp_table, arp_entry->ip_addr.ip_addr);

  if(old_arp_entry && 
	memcmp(old_arp_entry, arp_entry, sizeof(arp_entry_t)) == 0)
    return false;

  if(old_arp_entry) {
    delete_arp_table_entry(arp_table, old_arp_entry->ip_addr.ip_addr);
  }

  glthread_node_init(&arp_entry->arp_glue); 
  glthread_add_next(&arp_table->arp_entries, &arp_entry->arp_glue); 
 
  return true;
}

void
dump_arp_table(arp_table_t *arp_table) {

 arp_entry_t* arp_entry;
 gl_thread_t* curr;

 ITERATE_GLTHREAD_BEGIN(&arp_table->arp_entries, curr) {
   arp_entry = arp_glue_to_arp_entry(curr);
   printf("IP: %s, SRC MAC: %u:%u:%u:%u:%u:%u, OIF: %s",
	  arp_entry->ip_addr.ip_addr,
	  arp_entry->mac_addr.mac[0],
	  arp_entry->mac_addr.mac[1],
	  arp_entry->mac_addr.mac[2],
	  arp_entry->mac_addr.mac[3],
	  arp_entry->mac_addr.mac[4],
	  arp_entry->mac_addr.mac[5],
 	  arp_entry->oif); 
 }ITERATE_GLTHREAD_END(&arp_table->arp_entries, curr);

}

void
arp_table_update_from_arp_reply(arp_table_t *arp_table,
                                arp_hdr_t *arp_hdr, interface_t *iif) {
  unsigned int src_ip = 0;
  src_ip = arp_hdr->src_ip;
  assert(arp_hdr->op_code == ARP_REPLY);
  arp_entry_t* arp_entry = calloc(1, sizeof(arp_entry_t));
  inet_ntop(AF_INET, &src_ip, &arp_entry->ip_addr.ip_addr, 16);
  arp_entry->ip_addr.ip_addr[15] = '\0';
  strncpy(arp_entry->mac_addr.mac, arp_hdr->src_mac.mac, sizeof(mac_add_t));
  strncpy(arp_entry->oif, iif->if_name, IF_NAME_SIZE); 
  arp_entry->oif[IF_NAME_SIZE] = '\0';
  
  if(arp_table_entry_add(arp_table, arp_entry) == false) {
    free(arp_entry);
  }
}

/*A routine to resolve ARP out of interface*/

static void
send_arp_reply_msg(ethernet_hdr_t* ethernet_hdr, interface_t *oif) {

 arp_hdr_t* arp_hdr = (arp_hdr_t*)ethernet_hdr->payload;
 ethernet_hdr_t* ethernet_hdr_reply = calloc(1, sizeof(ethernet_hdr_t) + sizeof(arp_hdr_t));

 memcpy(ethernet_hdr_reply->dst_addr.mac, arp_hdr->src_mac.mac, sizeof(mac_add_t));
 memcpy(ethernet_hdr_reply->src_addr.mac, INTF_MAC(oif), sizeof(mac_add_t));
 ethernet_hdr_reply->type = ARP_MSG;
 ethernet_hdr_reply->FCS = 0;

 arp_hdr_t* arp_hdr_reply = (arp_hdr_t*)ethernet_hdr_reply->payload;
 arp_hdr_reply->hw_type        = 1;
 arp_hdr_reply->proto_type     = 0x0800;
 arp_hdr_reply->hw_addr_len    = sizeof(mac_add_t);
 arp_hdr_reply->proto_addr_len = 4;
 arp_hdr_reply->op_code        = ARP_REPLY;

 memcpy(arp_hdr_reply->src_mac.mac, INTF_MAC(oif), sizeof(mac_add_t));
 memcpy(arp_hdr_reply->dst_mac.mac, arp_hdr->src_mac.mac, sizeof(mac_add_t));

 inet_pton(AF_INET, INTF_IP(oif), &arp_hdr_reply->src_ip);
 arp_hdr_reply->src_ip = htonl(arp_hdr_reply->src_ip);
 arp_hdr_reply->dst_ip = arp_hdr->src_ip;

 send_pkt_out((char *)ethernet_hdr_reply, sizeof(ethernet_hdr_t) + sizeof(arp_hdr_t),
                  oif);
 free(ethernet_hdr_reply);

}

static void
process_arp_reply_request(node_t* node, interface_t* iif, 
			 ethernet_hdr_t* ethernet_hdr) {

  printf("ARP Reply msg recieved on interface: %s of node: %s\n", iif->if_name, node->node_name);
  arp_table_update_from_arp_reply(NODE_ARP_TABLE(node), 
				(arp_hdr_t *)ethernet_hdr->payload, iif);
}

static void
process_arp_broadcast_request(node_t* node, interface_t* iif, 
				ethernet_hdr_t* ethernet_hdr) {

  printf("ARP Broadcast req recieved on interface: %s of node : %s\n", iif->if_name, node->node_name);
  char ip_addr[16];
  arp_hdr_t* arp_hdr = (arp_hdr_t*)ethernet_hdr->payload;
  unsigned int dst_ip = htonl(arp_hdr->dst_ip);
  inet_ntop(AF_INET, &dst_ip, ip_addr, 16);
  ip_addr[15] = '\0';
  
  if(strncmp(ip_addr, INTF_IP(iif), 16) != 0) { 
    printf("%s: ARP Broadcast req msg dropped, destination IP: %s didnt match with interface IP: %s", 
		node->node_name, ip_addr, INTF_IP(iif));
    return;
  }
  send_arp_reply_msg(ethernet_hdr, iif);
 
}

void
send_arp_broadcast_request(node_t *node,
                           interface_t *oif,
                           char *ip_addr) {

  /*Take memory which can accomaodate Ethernet header and payload which is ARP hdr in this case*/
  ethernet_hdr_t* ethernet_hdr = calloc(1, sizeof(ethernet_hdr_t) + sizeof(arp_hdr_t));
   
  if(oif == NULL) {
    oif  = node_get_matching_subnet_interface(node, ip_addr);
    if(oif == NULL) {
      printf("Error: For node: %s Invalid subnet! ARP can't be resolved for IP: %s\n", 
		node->node_name, ip_addr);
      return;
    }
  }

  layer2_fill_with_broadcast_mac(ethernet_hdr->dst_addr.mac);
  memcpy(ethernet_hdr->src_addr.mac, INTF_MAC(oif), sizeof(mac_add_t));
  ethernet_hdr->type = ARP_MSG;
  ethernet_hdr->FCS = 0;

  arp_hdr_t* arp_hdr = (arp_hdr_t*)ethernet_hdr->payload;
  arp_hdr->hw_type        = 1;
  arp_hdr->proto_type     = 0x0800;
  arp_hdr->hw_addr_len    = sizeof(mac_add_t);
  arp_hdr->proto_addr_len = 4;
  arp_hdr->op_code	  = ARP_BROADCAST_REQ;
  
  memcpy(arp_hdr->src_mac.mac, INTF_MAC(oif), sizeof(mac_add_t));
  
  memset(arp_hdr->dst_mac.mac, 0,  sizeof(mac_add_t));
  
  inet_pton(AF_INET, INTF_IP(oif), &arp_hdr->src_ip);
  arp_hdr->src_ip = htonl(arp_hdr->src_ip);
  
  inet_pton(AF_INET, ip_addr, &arp_hdr->dst_ip);
  arp_hdr->dst_ip = htonl(arp_hdr->dst_ip);
 
  /*STEP 3 : Now dispatch the ARP Broadcast Request Packet out of interface*/
  send_pkt_out((char *)ethernet_hdr, sizeof(ethernet_hdr_t) + sizeof(arp_hdr_t),
                  oif);

  free(ethernet_hdr);
}



