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

PS: Check Testing.txt file for testing related logs.


