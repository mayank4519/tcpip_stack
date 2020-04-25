#ifndef __UTILS__
#define __UTILS__

#define IS_MAC_BROADCAST_ADDR(mac) \
  (mac[0] == 0XFF && mac[1] == 0XFF && mac[2] == 0XFF &&  \
  mac[3] == 0XFF && mac[4] == 0XFF && mac[5] == 0XFF)

void
layer2_fill_with_broadcast_mac(char *mac);


void
apply_mask(char* ip, char mask, char *nw_ip);

#endif
