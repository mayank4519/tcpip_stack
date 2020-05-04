#ifndef __COMM__
#define __COMM__
#define MAX_PACKET_BUFFER_SIZE 2048

int
pkt_receive(node_t* node, interface_t* intf,
            char* recv_data,
            int size);

#endif
