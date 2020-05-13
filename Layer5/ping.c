/* This fn sends a dummy packet to test L3 and L2 routing
 *  * in the project. We send dummy Packet starting from Network
 *   * Layer on node 'node' to destination address 'dst_ip_addr'
 *    * using below fn*/

#include "../graph.h"
#include "../Layer3/Layer3.h"
#include "../glthreads.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> /*for AF_INET*/
#include <arpa/inet.h>  /*for inet_pton and inet_ntop*/
#include "tcpconst.h"

extern void
demote_pkt_to_layer3(node_t* node,
                        char *pkt, unsigned int size,
                        int protocol_number,
                        unsigned int dest_ip_address);

void
layer5_ping_fn(node_t *node, char *dst_ip_addr) {

  unsigned int dst_ip;
  printf("LAYER5: Src node : %s, Intiate ping, ip : %s\n", node->node_name, dst_ip_addr);

  inet_pton(AF_INET, dst_ip_addr, &dst_ip);
  dst_ip = htonl(dst_ip);
  demote_pkt_to_layer3(node, NULL, 0, ICMP_PRO, dst_ip);

}

void
layer3_ero_ping_fn(node_t *node, char *dst_ip_addr,
                            char *ero_ip_address) {
}

