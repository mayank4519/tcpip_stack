#include "graph.h"
#include "comm.h"
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h> /*for struct hostent*/

static unsigned int udp_port_number = 50000;

static char recv_buffer[MAX_PACKET_BUFFER_SIZE];
static char send_buffer[MAX_PACKET_BUFFER_SIZE];

static unsigned int
get_next_udp_port_number() {

  return udp_port_number++;
}

void
init_udp_socket(node_t* node) {

  if(node->udp_port_number != NULL) return;

  node->udp_port_number = get_next_udp_port_number();
  
  int sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP );

   if (sock_fd == -1) {
     printf("Socket creation failed for node: %s\n", node->node_name);
     return -1;
   }

  const int trueFlag = 1;
  if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &trueFlag, sizeof(int)) < 0) {
    printf("setsockopt() API failed\n");
    return -1;
  }

  struct sockaddr_in node_addr;
  node_addr.sin_family = AF_INET;
  node_addr.sin_port  = node->udp_port_number;
  node_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock_fd, (struct sockaddr *)&node_addr, sizeof(struct sockaddr)) == -1) {
    printf("Socket bind failed for node: %s. Error-no: %d\n", node->node_name, errno);
    return;
  }

  node->udp_sock_fd = sock_fd;

}

static int
_send_pkt_out(int sock, char* aux_data, int size, int dest_port) {

 int rc;
 struct sockaddr_in dest_addr;
 struct hostent *host = (struct hostent *) gethostbyname("127.0.0.1");

 dest_addr.sin_family = AF_INET;
 dest_addr.sin_port   = dest_port;
 dest_addr.sin_addr   = *((struct in_addr *)host->h_addr);

 rc = sendto(sock, aux_data, size, 0,
	    (struct sockaddr *)&dest_addr, sizeof(struct sockaddr));

 return rc;
}

/*Public API to be used by other modules*/
int
send_pkt_out(char* pkt, unsigned int pkt_size,
            interface_t *interface) {
 
 int rc;
 node_t* origin_node = interface->attr_node;
 link_t* link        = interface->link;
 interface_t* dest_interface = &link->intf1 == interface ? \
				&link->intf2 : &link->intf1;

 node_t* dest_node = dest_interface->attr_node;

 int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP );

 if(sock < 0){
     printf("Error : Sending socket Creation failed , errno = %d", errno);
     return -1;
 }


 int dst_udp_port = dest_node->udp_port_number; 

 memset(send_buffer, 0, MAX_PACKET_BUFFER_SIZE);
 strncpy(send_buffer, dest_interface->if_name, IF_NAME_SIZE);
 send_buffer[IF_NAME_SIZE] = '\0';

 memcpy(send_buffer + IF_NAME_SIZE, pkt, pkt_size);
 send_buffer[MAX_PACKET_BUFFER_SIZE] = '\0';

 rc = _send_pkt_out(sock, send_buffer, pkt_size + IF_NAME_SIZE,
	      dst_udp_port); 
 
 close(sock);
 return rc;
}

extern void
layer2_frame_recv(node_t *node, interface_t *interface,
                     char *pkt, unsigned int pkt_size);

int
pkt_receive(node_t* node, interface_t* intf,
	    char* pkt,
	    int pkt_size)
{

  layer2_frame_recv(node, intf, pkt, pkt_size ); 
  return 0;
}

static void
_pkt_receive(node_t* node,
	     char* pkt_with_aux_data,
             unsigned int recv_bytes)
{

  char* get_intf_name =  pkt_with_aux_data;
  interface_t* recv_intf = get_node_intf_by_name(node, get_intf_name);
  
  if(recv_intf == NULL) {
    printf("Error: Packet recieved on unknown interface\n");
    return;
  }
  
  pkt_receive(node, recv_intf, pkt_with_aux_data + IF_NAME_SIZE, 
		recv_bytes - IF_NAME_SIZE);
}

static void*
_network_start_pkt_receiver_thread(void *arg) {

 gl_thread_t* curr;
 node_t* node;

 int max_sock_fd = -1;
 struct sockaddr sender_addr;
 int addr_len, recv_bytes;

 graph_t* topo = (graph_t*) arg;

 fd_set active_sock_fd_set,
        backup_sock_fd_set;

 ITERATE_GLTHREAD_BEGIN(&topo->node_list, curr) {

   node = graph_glue_to_node(curr);
   if(node == NULL) continue;

   if(node->udp_sock_fd > max_sock_fd)
     max_sock_fd = node->udp_sock_fd;

   FD_SET(node->udp_sock_fd, &backup_sock_fd_set);

 } ITERATE_GLTHREAD_END(&topo->node_list, curr);

 while(1) {

  memcpy(&active_sock_fd_set, &backup_sock_fd_set, sizeof(fd_set));
   
  select(max_sock_fd+1, &active_sock_fd_set, NULL, NULL, NULL);

  ITERATE_GLTHREAD_BEGIN(&topo->node_list, curr) {

    node = graph_glue_to_node(curr);
    if(FD_ISSET(node->udp_sock_fd, &active_sock_fd_set)) {

      memset(recv_buffer, 0, MAX_PACKET_BUFFER_SIZE);

      recv_bytes = recvfrom(node->udp_sock_fd, (char*)recv_buffer, MAX_PACKET_BUFFER_SIZE,
		0, (struct sockaddr *)&sender_addr, &addr_len);

      _pkt_receive(node, recv_buffer, recv_bytes);
    }

  } ITERATE_GLTHREAD_END(&topo->node_list, curr); 
 }
 
}

void
network_start_pkt_receiver_thread(graph_t* topo) {

 pthread_attr_t attr;
 pthread_t recv_pkt_thread;

 pthread_attr_init(&attr);
 pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

 pthread_create(&recv_pkt_thread, &attr,
		_network_start_pkt_receiver_thread,
		(void*)topo);
}

int
send_pkt_flood(node_t* node,
               interface_t *recv_intf,
               char* pkt, unsigned int pkt_size) {

  interface_t* intf;
  unsigned int i = 0;
  for(; i < MAX_INTF_PER_NODE; i++) {
    intf = node->intf[i];

    if(intf == NULL)
	return 0;
    if(strncmp(intf->if_name, recv_intf->if_name, IF_NAME_SIZE) != 0) {
      send_pkt_out(pkt, pkt_size, intf);
    }
  }
  return 0;
}
