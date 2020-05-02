#include <stdlib.h>
#include <stdio.h>
#include "../graph.h"
#include "Layer2.h"
#include "../glthreads.h"

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

  //glthread_node_init(&(mac_entry->mac_entry_glue));
  rc = mac_table_entry_add(node->node_nw_prop.mac_table, mac_entry);
  
  if(rc == false)
    free(mac_entry);
}

static void
l2_switch_forward_frame(node_t* node,
			interface_t *recv_intf,
			char* pkt, unsigned int pkt_size) {

  ethernet_hdr_t *ethernet_hdr = (ethernet_hdr_t *)pkt;
  if (IS_MAC_BROADCAST_ADDR(ethernet_hdr->dst_addr.mac)) {
    send_pkt_flood(node, recv_intf, pkt, pkt_size);
    return;
  } 

  /*If it's not a broadcast message.*/
  mac_table_entry_t* mac_entry = mac_table_entry_lookup(NODE_MAC_TABLE(node), ethernet_hdr->dst_addr.mac);  

  
  /*Check if there is already a MAC entry present in MAC table.
 * If not, then flood to all other ports.*/
  if(mac_entry == NULL) {
    send_pkt_flood(node, recv_intf, pkt, pkt_size);
    return;
  }

  char* oif_name = mac_entry->oif_name;
  interface_t* oif = get_node_intf_by_name(node, oif_name);
  if(oif == NULL)
    return;

  send_pkt_out(pkt, pkt_size, oif); 

}

void
l2_switch_recv_frame(interface_t *interface,
                     char *pkt, unsigned int pkt_size) {

  ethernet_hdr_t *ethernet_hdr = (ethernet_hdr_t *)pkt;
  
  node_t* node = interface->attr_node->node_name;

  char* src_mac = (char*)ethernet_hdr->src_addr.mac;
  l2_switch_perform_mac_learning(node, src_mac, interface->if_name);
  l2_switch_forward_frame(node, interface, pkt, pkt_size);
 
}

