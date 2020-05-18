#include "Layer2.h"
#include <stdlib.h>
#include <sys/socket.h>
#include "../graph.h"
#include <stdio.h>
#include <assert.h>
#include "tcpconst.h"

void
init_arp_table(arp_table_t** arp_table) {

  *arp_table = calloc(1, sizeof(arp_table_t));
  glthread_node_init(&((*arp_table)->arp_entries)); 
}

void
interface_set_l2_mode(node_t* node, 
                     interface_t* interface, char* l2mode) {

  intf_l2_mode_t intf_l2_mode;
  if(strncmp(l2mode, "access", strlen("access")) == 0)
    intf_l2_mode = ACCESS;
  
  else if(strncmp(l2mode, "trunk", strlen("trunk")) == 0)
    intf_l2_mode = TRUNK;

  else
    assert(0);

  if(IS_INTF_L3_MODE(interface)){
    interface->intf_nw_prop.is_ip_config = false;
    INTF_L2_MODE(interface) = intf_l2_mode;
    return;
  }

  if(INTF_L2_MODE(interface) == intf_l2_mode) {
    return;
  }

  if(INTF_L2_MODE(interface) == L2_MODE_UNKNOWN) {
    INTF_L2_MODE(interface) = intf_l2_mode;
    return;
  }

  if(INTF_L2_MODE(interface) == ACCESS && 
	intf_l2_mode == TRUNK) {
    INTF_L2_MODE(interface) = intf_l2_mode;
    return;
  }

  if(INTF_L2_MODE(interface) == TRUNK && 
	intf_l2_mode == ACCESS) {

    INTF_L2_MODE(interface) = intf_l2_mode;

    unsigned int i = 0;

    for ( ;i < MAX_VLAN_MEMBERSHIP; i++) {
        interface->intf_nw_prop.vlans[i] = 0;
    }

    return;
  }

}

void
node_set_intf_l2_mode(node_t* node, char* iif,
                        intf_l2_mode_t l2_mode) {

  interface_t* interface = get_node_intf_by_name(node, iif);
  assert(interface); 
  interface_set_l2_mode(node, interface, intf_l2_mode_str(l2_mode));
}

static bool
l2_frame_recv_qualify_on_interface(interface_t *interface,
                                    ethernet_hdr_t *ethernet_hdr,
					unsigned int *output_vlan){

  vlan_802q_hdr_t *vlan_802q_hdr =
                        is_pkt_vlan_tagged(ethernet_hdr);

  if(!IS_INTF_L3_MODE(interface) &&
      INTF_L2_MODE(interface) == L2_MODE_UNKNOWN) { 
       printf("%s: l2 mode unknown\n", __FUNCTION__);
	return false; //CASE 1
  }

  if (INTF_L2_MODE(interface) == ACCESS) {

   if(NULL == vlan_802q_hdr) {
     if (interface->intf_nw_prop.vlans[0]) {
       *output_vlan = interface->intf_nw_prop.vlans[0];
       return true; //CASE 2
     }
     else
	return false; //CASE 3
   }

   else {
     if(vlan_802q_hdr->tci_vid == interface->intf_nw_prop.vlans[0])
	return true; //CASE 4
     else
	return false; //CASE 5
   }
  }

  if(INTF_L2_MODE(interface) == TRUNK) {
    if(vlan_802q_hdr == NULL)
      return false; // CASE 6

    else {
      return (is_trunk_interface_vlan_enabled(interface, vlan_802q_hdr->tci_vid) == true) ? 
        true : false; // CASE 7 8
    }
  }

  if(IS_INTF_L3_MODE(interface) &&
	vlan_802q_hdr)
	return false; //CASE 9

  if(IS_INTF_L3_MODE(interface) &&
	!vlan_802q_hdr &&
	memcmp(ethernet_hdr->dst_addr.mac, INTF_MAC(interface), sizeof(mac_add_t)) == 0)
    return true; //CASE 10

  if(IS_INTF_L3_MODE(interface) &&
	!vlan_802q_hdr &&
	IS_MAC_BROADCAST_ADDR(ethernet_hdr->dst_addr.mac))
    return true; //CASE 11

  return false; //CASE 12
}

extern void
promote_pkt_to_layer3(node_t* node,
                        interface_t* intf,
                        char* pkt,
                        unsigned int pkt_size,
                        int L3_protocol_number);

void
promote_pkt_to_layer2(node_t *node, interface_t *interface,
			ethernet_hdr_t *ethernet_hdr,  unsigned int pkt_size) {

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
      case ETH_IP:
      {
	promote_pkt_to_layer3(node, interface, 
			GET_ETHERNET_HDR_PAYLOAD(ethernet_hdr),
			pkt_size - GET_ETH_HDR_SIZE_EXCL_PAYLOAD(ethernet_hdr),
			ethernet_hdr->type);
      }
      break;
      default:
        printf("Not an ARP msg\n");
        break;
    }
}

void
layer2_frame_recv(node_t *node, interface_t *interface,
                     char *pkt, unsigned int pkt_size){

    ethernet_hdr_t *ethernet_hdr = (ethernet_hdr_t *)pkt;
    unsigned int vlan_id_to_tag = 0;

    if(l2_frame_recv_qualify_on_interface(interface, ethernet_hdr, &vlan_id_to_tag) == false){

        printf("L2 Frame Rejected on interface: %s of node: %s\n", interface->if_name, node->node_name);
        return;
    }

    printf("L2 Frame Accepted on interface: %s of node: %s\n", interface->if_name, node->node_name);

    /*Handle Reception of a L2 Frame on L3 Interface*/
    if(IS_INTF_L3_MODE(interface)) {

	promote_pkt_to_layer2(node, interface, ethernet_hdr, pkt_size);
    }
    else if(INTF_L2_MODE(interface) == TRUNK ||
		INTF_L2_MODE(interface) == ACCESS) {

     unsigned int new_pkt_size = 0;
     if (vlan_id_to_tag) {
       pkt = tag_pkt_with_vlan_id((ethernet_hdr_t*)pkt,
                            pkt_size, vlan_id_to_tag,
                            &new_pkt_size); 
	assert(new_pkt_size != pkt_size);
     }

	l2_switch_recv_frame(interface, pkt, vlan_id_to_tag ?  new_pkt_size : pkt_size);
    }
    else
	return;
    
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

 delete_arp_entry(arp_entry);

}

bool
arp_table_entry_add(arp_table_t *arp_table, arp_entry_t *arp_entry,
		gl_thread_t **arp_pending_list) {

  arp_entry_t* old_arp_entry = arp_table_lookup(arp_table, arp_entry->ip_addr.ip_addr);

  //CASE 1: If arp entry doesn't exist
  if(!old_arp_entry) {
    glthread_add_next(&arp_table->arp_entries, 
			&arp_entry->arp_glue);
    return true;
  }

  //CASE 2: If arp entry exist and new entry are same.
  if(old_arp_entry &&
	IS_ARP_ENTRIES_EQUAL(old_arp_entry, arp_entry)) {
    return false;
  }

  //CASE 3: If arp entry exist and not same
  if(old_arp_entry && !arp_entry_sane(old_arp_entry)) {
    delete_arp_entry(old_arp_entry);
    glthread_node_init(&arp_entry->arp_glue);
    glthread_add_next(&arp_table->arp_entries, &arp_entry->arp_glue);
    return true;
  }

  //CASE 4: If both old and new arp entry are sane then move the pending arp list from new to old arp entry.
  if(old_arp_entry &&
	arp_entry_sane(old_arp_entry) &&
	arp_entry_sane(arp_entry)) {

   if(!IS_GLTHREAD_LIST_EMPTY(&arp_entry->arp_pending_list))  {
     
     glthread_add_next(&old_arp_entry->arp_pending_list,
			arp_entry->arp_pending_list.right);
  }
     if(arp_pending_list)
       *arp_pending_list = 
		&old_arp_entry->arp_pending_list;
  
   return false;
  }

  //CASE 5: If old arp entry is sane, but new one is not sane then copy all the content of new ARP entry to old one.
  if(old_arp_entry &&
	arp_entry_sane(old_arp_entry) &&
        !arp_entry_sane(arp_entry)) {
  
    strncpy(old_arp_entry->mac_addr.mac, arp_entry->mac_addr.mac, 6);
    strncpy(old_arp_entry->oif, arp_entry->oif, IF_NAME_SIZE);
    old_arp_entry->oif[IF_NAME_SIZE - 1] = '\0';

    if(arp_pending_list)
      *arp_pending_list = 
		&old_arp_entry->arp_pending_list;

    return false;
  }

  return false;
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

static void
process_arp_pending_entry(node_t* node,
			interface_t* iif,
			arp_entry_t *arp_entry,
			arp_pending_entry_t *arp_pending_entry) 
{
  arp_pending_entry->cb(node, iif, 
			arp_entry, arp_pending_entry);
}

static void
delete_arp_pending_entry(arp_pending_entry_t* 
		arp_pending_entry) {

  remove_glthread(&arp_pending_entry->arp_pending_entry_glue);
  free(arp_pending_entry);
}


void
delete_arp_entry(arp_entry_t* arp_entry) {

  gl_thread_t* curr;
  arp_pending_entry_t *arp_pending_entry;
  remove_glthread(&arp_entry->arp_glue);

  ITERATE_GLTHREAD_BEGIN(&arp_entry->arp_pending_list
			, curr){

    arp_pending_entry = arp_pending_entry_glue_to_arp_pending_entry(curr);
    delete_arp_pending_entry(arp_pending_entry);

  } ITERATE_GLTHREAD_END(&arp_entry->arp_pending_list, 
	curr);

  free(arp_entry);
	
}

void
arp_table_update_from_arp_reply(arp_table_t *arp_table,
                                arp_hdr_t *arp_hdr, interface_t *iif) {

  unsigned int src_ip = 0;
  assert(arp_hdr->op_code == ARP_REPLY);
  
  gl_thread_t* arp_pending_list = NULL;

  src_ip = htonl(arp_hdr->src_ip);
  arp_entry_t* arp_entry = calloc(1, sizeof(arp_entry_t));
  inet_ntop(AF_INET, &src_ip, &arp_entry->ip_addr.ip_addr, 16);
  arp_entry->ip_addr.ip_addr[15] = '\0';
  arp_entry->is_sane = false;
  strncpy(arp_entry->mac_addr.mac, arp_hdr->src_mac.mac, sizeof(mac_add_t));
  strncpy(arp_entry->oif, iif->if_name, IF_NAME_SIZE); 
  arp_entry->oif[IF_NAME_SIZE-1] = '\0';
  
  bool rc = arp_table_entry_add(arp_table, arp_entry, 
		&arp_pending_list);

  gl_thread_t *curr;
  arp_pending_entry_t *arp_pending_entry;

  if(arp_pending_list) {
    
    ITERATE_GLTHREAD_BEGIN(arp_pending_list, curr) {

      arp_pending_entry = 
	arp_pending_entry_glue_to_arp_pending_entry(curr);

      remove_glthread(&arp_pending_entry->arp_pending_entry_glue);

      process_arp_pending_entry(iif->attr_node, iif, 
		arp_entry, arp_pending_entry);            

      delete_arp_pending_entry(arp_pending_entry);

    }ITERATE_GLTHREAD_END(arp_pending_list, curr);

   (arp_pending_list_to_arp_entry(arp_pending_entry))->is_sane = false; 
  }
  if (rc == false)
    delete_arp_entry(arp_entry);
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

 arp_hdr_t* arp_hdr_reply = (arp_hdr_t*)(ethernet_hdr_reply->payload);

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

  printf("ARP reply recieved on interface: %s of node: %s\n", iif->if_name, node->node_name);
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
    printf("%s: ARP Broadcast req msg dropped, destination IP: %s didnt match with interface IP: %s\n", 
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

ethernet_hdr_t*
tag_pkt_with_vlan_id(ethernet_hdr_t* ethernet_hdr,
                        unsigned int total_pkt_size,
                        int vlan_id,
                        unsigned int *new_pkt_size) {

  vlan_802q_hdr_t* vlan_802q_hdr = is_pkt_vlan_tagged(ethernet_hdr);

  if(vlan_802q_hdr != NULL) {
    vlan_802q_hdr->tci_vid = vlan_id;
    *new_pkt_size = total_pkt_size;
    return ethernet_hdr;
  }

  /*If the packet doesn't have a vlan header. Do following*/

  /*Take a backup of ethernet hdr without payload and FCS*/
  ethernet_hdr_t ethernet_hdr_old;
  memcpy((char*)&ethernet_hdr_old, 
	(char*)ethernet_hdr, 
	ETH_HDR_SZ_EXCL_PAYLOAD - sizeof(ethernet_hdr->FCS));

  /*Shift the pointer back by size of vlan header(4 bytes)*/
  vlan_ethernet_hdr_t* vlan_ethernet_hdr = (vlan_ethernet_hdr_t*)((char*)ethernet_hdr - sizeof(vlan_802q_hdr_t));
 
  /*Intialize the data before payload to zero*/ 
  memset((char*)vlan_ethernet_hdr, 0, VLAN_ETH_HDR_SZ_EXCL_PAYLOAD - sizeof(vlan_ethernet_hdr->FCS));

  memcpy(vlan_ethernet_hdr->dst_addr.mac, ethernet_hdr_old.dst_addr.mac, sizeof(mac_add_t));
  memcpy(vlan_ethernet_hdr->src_addr.mac, ethernet_hdr_old.src_addr.mac, sizeof(mac_add_t));

  vlan_ethernet_hdr->type = ethernet_hdr_old.type;

  vlan_ethernet_hdr->vlan_802q_hdr.tpid = 0X8100;
  vlan_ethernet_hdr->vlan_802q_hdr.tci_pcp  = 0;
  vlan_ethernet_hdr->vlan_802q_hdr.tci_dei  = 0;
  vlan_ethernet_hdr->vlan_802q_hdr.tci_vid  = (short)vlan_id;

  *new_pkt_size = total_pkt_size + sizeof(vlan_802q_hdr_t);
  
  return (ethernet_hdr_t*)vlan_ethernet_hdr;
}

ethernet_hdr_t*
untag_pkt_with_vlan_id(ethernet_hdr_t* ethernet_hdr,
                        unsigned int total_pkt_size,
                        unsigned int *new_pkt_size) {

  vlan_802q_hdr_t* vlan_802q_hdr = is_pkt_vlan_tagged(ethernet_hdr);

  if (vlan_802q_hdr == NULL) {
    printf("%s(), packet doesnt have a vlan header so no need to remove\n", __FUNCTION__);
    new_pkt_size = total_pkt_size;
    return ethernet_hdr;
  }

  vlan_ethernet_hdr_t vlan_ethernet_hdr_old;
  memcpy((char*)&vlan_ethernet_hdr_old,
	(char*)ethernet_hdr,
	VLAN_ETH_HDR_SZ_EXCL_PAYLOAD - sizeof(ethernet_hdr->FCS));

  ethernet_hdr = (ethernet_hdr_t*)((char*)ethernet_hdr + sizeof(vlan_802q_hdr_t));

  memcpy(ethernet_hdr->dst_addr.mac, vlan_ethernet_hdr_old.dst_addr.mac, sizeof(mac_add_t));
  memcpy(ethernet_hdr->src_addr.mac, vlan_ethernet_hdr_old.src_addr.mac, sizeof(mac_add_t));

  ethernet_hdr->type = vlan_ethernet_hdr_old.type;

  *new_pkt_size = total_pkt_size - sizeof(vlan_802q_hdr_t);
  return ethernet_hdr;
}

void
interface_set_vlan(node_t* node, interface_t* interface,
			unsigned int vlan) {

  if(IS_INTF_L3_MODE(interface)) {
	printf("Error: L3 mode configured.\n");
	return;
  }

  if(INTF_L2_MODE(interface) != TRUNK &&
	INTF_L2_MODE(interface) != ACCESS) {
     printf("Error: L2 mode not configured.\n");
     return;
  }

  if(INTF_L2_MODE(interface) == ACCESS) {
    interface->intf_nw_prop.vlans[0] = vlan;
  }

  if(INTF_L2_MODE(interface) == TRUNK) {

    unsigned int i=0;

    for ( ;i<MAX_VLAN_MEMBERSHIP; i++) {
	if (interface->intf_nw_prop.vlans[i] == 0) {
	  interface->intf_nw_prop.vlans[i] = vlan;
	  return;
	}
	else if(interface->intf_nw_prop.vlans[i] == vlan) 
	  return;	
    }
  }
}

void
node_set_intf_vlan_membership(node_t* node, char* intf_name,
				unsigned int vlan) {

  interface_t* interface = get_node_intf_by_name(node, intf_name);
  assert(interface);

  interface_set_vlan(node, interface, vlan);

}
static void
pending_arp_processing_callback_function(node_t* node,
                                interface_t* oif,
                                arp_entry_t* arp_entry,
                                arp_pending_entry_t* arp_pending_list) {

  ethernet_hdr_t* ethernet_hdr = (ethernet_hdr_t*)arp_pending_list->pkt;
  unsigned int pkt_size = arp_pending_list->pkt_size;
  memcpy(ethernet_hdr->dst_addr.mac, arp_entry->mac_addr.mac, sizeof(mac_add_t));
  memcpy(ethernet_hdr->src_addr.mac, INTF_MAC(oif), sizeof(mac_add_t));
  SET_COMMON_ETH_FCS(ethernet_hdr, pkt_size - GET_ETH_HDR_SIZE_EXCL_PAYLOAD(ethernet_hdr), 0);
  send_pkt_out((char *)ethernet_hdr, pkt_size, oif);

}

void
add_arp_pending_entry(arp_entry_t* arp_entry,
			arp_processing_fn cb,
			char* pkt,
			unsigned int pkt_size) {

 arp_pending_entry_t* arp_pending_entry = 
	calloc(1, sizeof(arp_pending_entry_t) + pkt_size);

  glthread_node_init(&arp_pending_entry->arp_pending_entry_glue); 
  
  arp_pending_entry->cb = cb;
  memcpy(arp_pending_entry->pkt, pkt, pkt_size);
  arp_pending_entry->pkt_size = pkt_size;

  glthread_add_next(&arp_entry->arp_pending_list,
		&arp_pending_entry->arp_pending_entry_glue);
}

arp_entry_t*
create_arp_sane_entry(arp_table_t* arp_table,
			char* ip_addr) {

  arp_entry_t* arp_entry = arp_table_lookup(arp_table, ip_addr);
  
  if(arp_entry)
	if(!arp_entry_sane(arp_entry)) {
	    assert(0);
    return arp_entry;
  }

  /*If ARP entry doesn't exist, create an ARP entry*/
  arp_entry = calloc(1, sizeof(arp_entry_t));
  strncpy(arp_entry->ip_addr.ip_addr, ip_addr, 16);
  arp_entry->ip_addr.ip_addr[15] = '\0';
  arp_entry->is_sane = true;
  glthread_node_init(&arp_entry->arp_pending_list);
  bool rc = arp_table_entry_add(arp_table, arp_entry, 0); 
  if(rc == false)
    assert(0); 

  return arp_entry;
}

extern bool
is_layer3_local_delivery(node_t *node, unsigned int dst_ip);

static void
l2_forward_ip_packet(node_t *node, unsigned int next_hop_ip,
                    char *outgoing_intf, ethernet_hdr_t *pkt,
                    unsigned int pkt_size){
  
  printf("LAYER2: Packet recieved on layer 2\n");

  interface_t *intf = NULL;
  char next_hop_ip_str[16] = {};
  ethernet_hdr_t* ethernet_hdr = (ethernet_hdr_t*)pkt;
  unsigned int ethernet_payload_size = pkt_size - ETH_HDR_SZ_EXCL_PAYLOAD;
  arp_entry_t* arp_entry = NULL;

  next_hop_ip = ntohl(next_hop_ip);
  inet_ntop(AF_INET, &next_hop_ip, next_hop_ip_str, 16);
  next_hop_ip_str[15] = '\0';

  if(outgoing_intf) {
    /* Case 1 : Forwarding Case
     * It means, L3 has resolved the nexthop, So its
     * time to L2 forward the pkt out of this interface*/
    intf = get_node_intf_by_name(node, outgoing_intf);
    assert(intf);

    arp_entry = arp_table_lookup(NODE_ARP_TABLE(node), next_hop_ip_str);

    if(arp_entry == NULL) {
      /*Time for ARP resolution*/
      arp_entry = create_arp_sane_entry(NODE_ARP_TABLE(node), 
				next_hop_ip_str);

      add_arp_pending_entry(arp_entry,
			pending_arp_processing_callback_function,
			(char*)pkt, pkt_size);
 
      send_arp_broadcast_request(node, intf, next_hop_ip_str);
      return;
    }
    else if(arp_entry_sane(arp_entry)) {
      add_arp_pending_entry(arp_entry, 
			pending_arp_processing_callback_function,
			(char*)pkt, pkt_size);
      return;
    }
    goto l2_frame_prepare ;
  }

  /*CASE 4: LOCAL DELIVERY
 * It's a self ping case*/
  if(is_layer3_local_delivery(node, next_hop_ip)) {
    promote_pkt_to_layer3(node, 0, 
			GET_ETHERNET_HDR_PAYLOAD(ethernet_hdr), 
			pkt_size - GET_ETH_HDR_SIZE_EXCL_PAYLOAD(ethernet_hdr), 
			ethernet_hdr->type);
    return;
  }

  /* case 2 : Direct host Delivery
  * L2 has to forward the frame to machine on local connected subnet */
  arp_entry = arp_table_lookup(NODE_ARP_TABLE(node), next_hop_ip_str);

  if(arp_entry == NULL) {
     /*Time for ARP resolution*/
    arp_entry = create_arp_sane_entry(NODE_ARP_TABLE(node),
                              next_hop_ip_str);

    add_arp_pending_entry(arp_entry,
                      pending_arp_processing_callback_function,
                       (char*)pkt, pkt_size);
  
    send_arp_broadcast_request(node, intf, next_hop_ip_str);
    return;
  }
  else if(arp_entry_sane(arp_entry)) {
      add_arp_pending_entry(arp_entry,
                        pending_arp_processing_callback_function,
                        (char*)pkt, pkt_size);
      return;
  }

  intf = node_get_matching_subnet_interface(node, next_hop_ip_str);

  if(intf == NULL) {
    	printf("%s : Error : Local matching subnet for IP : %s could not be found\n",
                    node->node_name, next_hop_ip_str);
        return;
  }

  l2_frame_prepare : 
    memcpy(ethernet_hdr->dst_addr.mac, arp_entry->mac_addr.mac, sizeof(mac_add_t));
    memcpy(ethernet_hdr->src_addr.mac, INTF_MAC(intf), sizeof(INTF_MAC(intf)));
    SET_COMMON_ETH_FCS(ethernet_hdr, ethernet_payload_size, 0);
    send_pkt_out((char*)ethernet_hdr, pkt_size, intf);
}

void layer2_pkt_recieve_from_top(node_t *node,
                        unsigned int next_hop_ip,
                        char *outgoing_intf,
                        char *pkt, unsigned int pkt_size,
                        int protocol_number) {

  assert(pkt_size < sizeof(((ethernet_hdr_t*)0)->payload));

  if(ETH_IP == protocol_number) {
    ethernet_hdr_t* empty_ethernet_hdr = ALLOC_ETH_HDR_WITH_PAYLOAD(pkt, pkt_size);
    empty_ethernet_hdr->type = ETH_IP;

    l2_forward_ip_packet(node, next_hop_ip,
			outgoing_intf, empty_ethernet_hdr, pkt_size + ETH_HDR_SZ_EXCL_PAYLOAD);
  }
 
}

/*An API used by layer 5/4/3 to send the packet down to layer 2*/
void
demote_pkt_to_layer2(node_t *node,
                        unsigned int next_hop_ip,
                        char *outgoing_intf,
                        char *pkt, unsigned int pkt_size,
                        int protocol_number) {

  layer2_pkt_recieve_from_top(node, next_hop_ip,
			      outgoing_intf, 
			      pkt, pkt_size,
			      protocol_number);
}

