#include "graph.h"

extern void
network_start_pkt_receiver_thread(graph_t*);

graph_t *
build_first_topo() {

#if 0
                          +----------+
                      0/4 |          |0/0
         +----------------+   R0_re  +---------------------------+
         |     40.1.1.1/24| 122.1.1.0|20.1.1.1/24                |
         |                +----------+                           |
         |                                                       |
         |                                                       |
         |                                                       |
         |40.1.1.2/24                                            |20.1.1.2/24
         |0/5                                                    |0/1
     +---+---+                                              +----+-----+
     |       |0/3                                        0/2|          |
     | R2_re +----------------------------------------------+    R1_re |
     |       |30.1.1.2/24                        30.1.1.1/24|          |
     +-------+                                              +----------+



#endif

  graph_t *topo = create_new_graph("Hello Generic Graph");
  node_t *R0_re = create_graph_node(topo, "R0_re");
  node_t *R1_re = create_graph_node(topo, "R1_re");
  node_t *R2_re = create_graph_node(topo, "R2_re");

  insert_link_between_two_nodes(R0_re, R1_re, "eth0/0", "eth0/1", 1);
  insert_link_between_two_nodes(R1_re, R2_re, "eth0/2", "eth0/3", 1);
  insert_link_between_two_nodes(R0_re, R2_re, "eth0/4", "eth0/5", 1);

  node_set_loopback_address(R0_re, "122.1.1.0");
  node_set_intf_ip_addres(R0_re, "eth0/4", "40.0.0.1", 24); 
  node_set_intf_ip_addres(R0_re, "eth0/0", "20.0.0.1", 24); 

  node_set_loopback_address(R2_re, "122.1.1.2");
  node_set_intf_ip_addres(R2_re, "eth0/3", "30.0.0.2", 24); 
  node_set_intf_ip_addres(R2_re, "eth0/5", "40.0.0.2", 24); 
  
  node_set_loopback_address(R1_re, "122.1.1.1");
  node_set_intf_ip_addres(R1_re, "eth0/1", "20.0.0.2", 24); 
  node_set_intf_ip_addres(R1_re, "eth0/2", "30.0.0.1", 24); 


  network_start_pkt_receiver_thread(topo);

  /*sleep(3);
  interface_t* intf = get_node_intf_by_name(R0_re, "eth0/4");
  interface_t* intf1 = get_node_intf_by_name(R1_re, "eth0/2");

  char *msg = "Working fine so far!";
  send_pkt_out(msg, strlen(msg), intf);
  send_pkt_out(msg, strlen(msg), intf1);*/


  return topo;
}

graph_t *
build_simple_l2_switch_topo(){

#if 0
                                       +-----------+
                                       |  H4       |
                                       | 122.1.1.4 |
                                       +----+------+
                                            |eth0/7 - 10.1.1.3/24
                                            |
                                            |eth0/1
                                       +----+----+                        +--------+
       +---------+                     |         |                        |        |
       |         |10.1.1.2/24          |   L2Sw  |eth0/2       10.1.1.1/24|  H3    |
       |  H1     +---------------------+         +------------------------+122.1.1.3|
       |122.1.1.1|eth0/5         eth0/4|         |                 eth0/6 |        |
       + --------+                     |         |                        |        |
                                       +----+----+                        +--------+
                                            |eth0/3
                                            |
                                            |
                                            |
                                            |10.1.1.4/24
                                            |eth0/8
                                      +----++------+
                                      |            |
                                      |   H2       |
                                      |122.1.1.2   |
                                      |            |
                                      +------------+

#endif

  graph_t *topo = create_new_graph("Simple L2 switch Graph");
  node_t *H1 = create_graph_node(topo, "H1");
  node_t *H2 = create_graph_node(topo, "H2");
  node_t *H3 = create_graph_node(topo, "H3");
  node_t *H4 = create_graph_node(topo, "H4");
  node_t *L2SW = create_graph_node(topo, "L2SW");

  insert_link_between_two_nodes(H1, L2SW, "eth0/5", "eth0/4", 1);
  insert_link_between_two_nodes(H2, L2SW, "eth0/8", "eth0/3", 1);
  insert_link_between_two_nodes(H3, L2SW, "eth0/6", "eth0/2", 1);
  insert_link_between_two_nodes(H4, L2SW, "eth0/7", "eth0/1", 1);
  
  node_set_loopback_address(H1, "122.1.1.1");
  node_set_intf_ip_addres(H1, "eth0/5", "10.1.1.2", 24);

  node_set_loopback_address(H2, "122.1.1.2");
  node_set_intf_ip_addres(H2, "eth0/8", "10.1.1.4", 24);

  node_set_loopback_address(H3, "122.1.1.3");
  node_set_intf_ip_addres(H3, "eth0/6", "10.1.1.1", 24);

  node_set_loopback_address(H4, "122.1.1.14");
  node_set_intf_ip_addres(H4, "eth0/7", "10.1.1.3", 24);

  node_set_intf_l2_mode(L2SW, "eth0/1", ACCESS);
  node_set_intf_l2_mode(L2SW, "eth0/2", ACCESS);
  node_set_intf_l2_mode(L2SW, "eth0/3", ACCESS);
  node_set_intf_l2_mode(L2SW, "eth0/4", ACCESS);

  network_start_pkt_receiver_thread(topo);

  return topo;
}
