#include <stdlib.h>
#include <stdio.h>
#include "../graph.h"
#include "Layer2.h"
#include "../glthreads.h"
#include "../comm.h"
#include "assert.h"

typedef struct mac_table_entry_ {
  mac_add_t mac;
  char oif_name[IF_NAME_SIZE];
  gl_thread_t mac_entry_glue;
}mac_table_entry_t;

typedef struct mac_table_ {
  gl_thread_t mac_entries;
}mac_table_t;

GLTHREAD_TO_STRUCT(mac_entry_glue_to_mac_table, mac_table_entry_t, mac_entry_glue);


void init_mac_table(mac_table_t** mac_table) {

  *mac_table = calloc(1, sizeof(mac_table_t));
  glthread_node_init(&((*mac_table)->mac_entries));
}

mac_table_entry_t*
mac_table_entry_lookup(mac_table_t* mac_table, char* mac) {

  gl_thread_t* curr;
  mac_table_entry_t* mac_entry;
  ITERATE_GLTHREAD_BEGIN(&mac_table->mac_entries,curr) {

    mac_entry = mac_entry_glue_to_mac_table(curr);
    if(strncmp(mac_entry->mac.mac, mac, 6) == 0)
	return mac_entry;
  }ITERATE_GLTHREAD_END(&mac_table->mac_entries,curr); 

  return NULL;
}

#define IS_MAC_TABLE_ENTRY_EQUAL(mac1, mac2) \
	(strncmp(mac1->mac.mac, mac2->mac.mac, sizeof(mac_add_t)) == 0 && \
		strncmp(mac1->oif_name, mac2->oif_name, IF_NAME_SIZE) == 0 )

bool
mac_table_entry_add(mac_table_t* mac_table, mac_table_entry_t* mac_entry) {

  mac_table_entry_t* old_mac_entry = mac_table_entry_lookup(mac_table, mac_entry->mac.mac);

  if(old_mac_entry != NULL && IS_MAC_TABLE_ENTRY_EQUAL(mac_entry, old_mac_entry)) {
    return false;
  }
 
  if(old_mac_entry != NULL)
    delete_mac_table_entry(mac_table, old_mac_entry->mac.mac);
 
  glthread_node_init(&(mac_entry->mac_entry_glue)); 
  glthread_add_next(&(mac_table->mac_entries), &(mac_entry->mac_entry_glue));
  return true;
}

void
clear_mac_table(mac_table_t* mac_table) {

  gl_thread_t* curr;
  mac_table_entry_t* mac_entry;
  ITERATE_GLTHREAD_BEGIN(&mac_table->mac_entries,curr) {

    mac_entry = mac_entry_glue_to_mac_table(curr);
    if(mac_table == NULL) {
      remove_glthread(&(mac_entry->mac_entry_glue));
      free(mac_entry);
    }
  }ITERATE_GLTHREAD_END(&mac_table->mac_entries,curr);

}

void
delete_mac_table_entry(mac_table_t* mac_table, char* mac) {

  mac_table_entry_t* mac_entry = mac_table_entry_lookup(mac_table, mac);
  if(mac_entry == NULL)
	return;

  remove_glthread(&(mac_entry->mac_entry_glue));
  free(mac_entry);
}

void
dump_mac_table(mac_table_t* mac_table) {

  gl_thread_t* curr;
  mac_table_entry_t* mac_entry;
  ITERATE_GLTHREAD_BEGIN(&mac_table->mac_entries,curr) {

    mac_entry = mac_entry_glue_to_mac_table(curr);
    if(mac_entry != NULL) {
      printf("Interface: %s, MAC %u:%u:%u:%u:%u:%u\n",
		mac_entry->oif_name, 
		mac_entry->mac.mac[0],
		mac_entry->mac.mac[1],
		mac_entry->mac.mac[2],
		mac_entry->mac.mac[3],
		mac_entry->mac.mac[4],
		mac_entry->mac.mac[5]);
    }
  }ITERATE_GLTHREAD_END(&mac_table->mac_entries,curr);

}

static void
l2_switch_perform_mac_learning(node_t* node, 
				    char* src_mac, char* intf_name) {

  
  bool rc;
  mac_table_entry_t* mac_entry = calloc(1, sizeof(mac_table_entry_t));
  memcpy(mac_entry->mac.mac, src_mac, sizeof(mac_add_t));
  strncpy(mac_entry->oif_name, intf_name, IF_NAME_SIZE);
  mac_entry->oif_name[IF_NAME_SIZE - 1] = '\0';

  rc = mac_table_entry_add(node->node_nw_prop.mac_table, mac_entry);
  
  if(rc == false)
    free(mac_entry);
}

bool
l2_switch_send_pkt_out(char* pkt, unsigned int pkt_size,
			interface_t *oif) {

  assert(!IS_INTF_L3_MODE(oif));
  
  if(INTF_L2_MODE(oif) == L2_MODE_UNKNOWN)
    return false;

  ethernet_hdr_t *ethernet_hdr = (ethernet_hdr_t *)pkt;

  vlan_802q_hdr_t* vlan_8021q_hdr = 
	is_pkt_vlan_tagged(ethernet_hdr); 

  switch(INTF_L2_MODE(oif)) {

    case ACCESS:
    {
/*CASE 1: Interface is operating in access mode and no vlan configured on interface and packet is also untagged.
 * Its a vlan unaware case. Send the packet out*/    
      if(oif->intf_nw_prop.vlans[0] == 0 &&
	vlan_8021q_hdr == NULL) {
	  //send_pkt_out(pkt, pkt_size, oif);
  	  //return true;
  	  assert(0);
      }
/*CASE 2:  Interface is operating in access mode and no vlan configured on interface and packet is tagged.
 * Drop the packet*/
      if(oif->intf_nw_prop.vlans[0] == 0 &&
        vlan_8021q_hdr != NULL) {
	return false;
      }

/*CASE 3:  Interface is operating in access mode and vlan configured on interface and packet is untagged.
 * Drop the packet*/
     if(oif->intf_nw_prop.vlans[0] && 
  	  vlan_8021q_hdr == NULL) {
	  return false;
     }

/*CASE 4:  Interface is operating in access mode and vlan configured on interface and packet is tagged.
 * If interface VLAN is same as packet vlan then forward the packet else drop*/
     if(oif->intf_nw_prop.vlans[0] &&
          vlan_8021q_hdr != NULL) {
      	    if(vlan_8021q_hdr->tci_vid == oif->intf_nw_prop.vlans[0]) {

              unsigned int new_pkt_size = 0;
	      ethernet_hdr = untag_pkt_with_vlan_id(ethernet_hdr,
						    pkt_size,
						    &new_pkt_size);

  	      send_pkt_out((char*)ethernet_hdr, new_pkt_size, oif);
	      return true;
            }
	    else
		return false;
      }
    }
    break;

    case TRUNK:
    {
      if(vlan_8021q_hdr != NULL) {
	if(is_trunk_interface_vlan_enabled(oif, vlan_8021q_hdr->tci_vid) == true) {
	  send_pkt_out(pkt, pkt_size, oif);
	  return true;
	}
      }
      return false;
    }
    break;

    case L2_MODE_UNKNOWN:
      break;

    default:
      ;
    return false;
  }
}

static void
l2_switch_flood_pkt_out(node_t* node, interface_t *exempted_intf, 
			char* pkt, unsigned int pkt_size) {

   interface_t *oif = NULL;

    unsigned int i = 0;

    char *pkt_copy = NULL;
    char *temp_pkt = calloc(1, MAX_PACKET_BUFFER_SIZE);

    pkt_copy = temp_pkt + MAX_PACKET_BUFFER_SIZE - pkt_size;

    for( ; i < MAX_INTF_PER_NODE; i++){

        oif = node->intf[i];
        if(!oif) break;
        if(oif == exempted_intf) continue;

        memcpy(pkt_copy, pkt, pkt_size);
        l2_switch_send_pkt_out(pkt_copy, pkt_size, oif);
    }
    free(temp_pkt);

}

static void
l2_switch_forward_frame(node_t* node,
			interface_t *recv_intf,
			char* pkt, unsigned int pkt_size) {

  ethernet_hdr_t *ethernet_hdr = (ethernet_hdr_t *)pkt;
  if (IS_MAC_BROADCAST_ADDR(ethernet_hdr->dst_addr.mac)) {
    l2_switch_flood_pkt_out(node, recv_intf, (char*)ethernet_hdr, pkt_size);
    return;
  } 

  /*If it's not a broadcast message.*/
  mac_table_entry_t* mac_entry = mac_table_entry_lookup(NODE_MAC_TABLE(node), ethernet_hdr->dst_addr.mac);  

  
  /*Check if there is already a MAC entry present in MAC table.
 * If not, then flood to all other ports.*/
  if(mac_entry == NULL) {
    l2_switch_flood_pkt_out(node, recv_intf, (char*)ethernet_hdr, pkt_size);
    return;
  }

  char* oif_name = mac_entry->oif_name;
  interface_t* oif = get_node_intf_by_name(node, oif_name);
  if(oif == NULL)
    return;

  l2_switch_send_pkt_out((char*)ethernet_hdr, pkt_size, oif); 

}

void
l2_switch_recv_frame(interface_t *interface,
                     char *pkt, unsigned int pkt_size) {

  ethernet_hdr_t *ethernet_hdr = (ethernet_hdr_t *)pkt;
  
  node_t* node = interface->attr_node->node_name;

  char* src_mac = (char*)ethernet_hdr->src_addr.mac;
  l2_switch_perform_mac_learning(node, src_mac, interface->if_name);
  l2_switch_forward_frame(node, interface, ethernet_hdr, pkt_size);
 
}

