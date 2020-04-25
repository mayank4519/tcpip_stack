#include "utils.h"
#include <sys/socket.h>

void
layer2_fill_with_broadcast_mac(char *mac) {
  mac[0] = 0XFF;
  mac[1] = 0XFF;
  mac[2] = 0XFF;
  mac[3] = 0XFF;
  mac[4] = 0XFF;
  mac[5] = 0XFF;
}

void
apply_mask(char* ip, char mask, char *nw_ip) {

  unsigned int binary_prefix;
  /*->inet_pton() API converts text format into host byte order means
  For e.g: 11.11.11.12(OX0B0B0B0C) is converted into 0X0C0B0B0B(i.e, host byte order or little endian format)*/
  inet_pton(AF_INET, ip, &binary_prefix);
  
  /*To convert little endian to big endian format, use htonl() API*/
  /*0X0C0B0B0B is converted into original format OX0B0B0B0C*/
  binary_prefix = htonl(binary_prefix);

  /*Get the network address*/
  for(int i=0; i < 32 - mask; i++) {
    binary_prefix = (binary_prefix & ((1 << i) ^ 0XFFFFFFFF));
  }

  /*Again convert OX0B0B0B00 to OX000B0B0B*/
  binary_prefix = htonl(binary_prefix);

  /*inet_ntop() API converts host byte order into text format*/
  inet_ntop(AF_INET, &binary_prefix, nw_ip, 16);
  nw_ip[15] = '\0';

}
