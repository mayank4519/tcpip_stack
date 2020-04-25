# tcpip_stack
Develop a TCP IP protocol stack

libCLI usage:
press . to get list of available commands.
type "show topology" which gives you the graph topology.

                               +----------+
                      0/4 |          |0/0
         +----------------+   R0_re  +---------------------------+
         |     40.0.0.1/24| 122.1.1.0|20.0.0.1/24                |
         |                +----------+                           |
         |                                                       |
         |                                                       |
         |                                                       |
         |40.0.0.2/24                                            |20.0.0.2/24
         |0/5                                                    |0/1
     +---+---+                                              +----+-----+
     |       |0/3                                        0/2|          |
     | R2_re +----------------------------------------------+    R1_re |
     |       |30.0.0.2/24                        30.0.0.1/24|          |
     +-------+                                              +----------+



1. Glue thread doubly linked list data struture is used to implement the graph.

2.Execute the binary and run below commands to test ARP functionality:

. --> This gives you the list of available CLI cmds 
show node R0_re arp
run node R0_re resolve-arp 20.0.0.2
show node R0_re arp

For example,
root@juniper> $ show node R0_re arp
Parse Success.

root@juniper> $ run node R0_re resolve-arp 20.0.0.2
Parse Success.
L2 Frame Accepted
ARP Broadcast req recieved on interface: eth0/1 of node : R1_re

root@juniper> $ L2 Frame Accepted
ARP Reply msg recieved on interface: eth0/0 of node: R0_re

root@juniper> $ show node R0_re arp
Parse Success.
IP: 20.0.0.2, SRC MAC: 56:53:48:57:53:49, OIF: eth0/0
root@juniper> $


