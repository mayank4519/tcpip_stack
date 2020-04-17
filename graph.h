#ifndef __GRAPH__
#define __GRAPH__
#include "glthreads.h"
#include "net.h"

#define IF_NAME_SIZE 16
#define NODE_NAME_SIZE 16
#define MAX_INTF_PER_NODE 10

/*Forward declaration*/
typedef struct node_ node_t;
typedef struct link_ link_t;

typedef struct interface_ {
  char if_name[IF_NAME_SIZE];
  struct link_t *link;
  struct node_ *attr_node;
  intf_nw_prop_t intf_nw_prop;
}interface_t;

typedef struct link_ {
  interface_t intf1;
  interface_t intf2;
  unsigned int cost;
};

typedef struct node_ {
  char node_name[NODE_NAME_SIZE];
  interface_t *intf[MAX_INTF_PER_NODE];
  node_nw_prop_t node_nw_prop;
  gl_thread_t graph_glue;
};

typedef struct graph_ {
  char topology_name[32];
  gl_thread_t node_list; 
}graph_t;

graph_t* 
create_new_graph(char* topology_name);

node_t*
create_graph_node(graph_t* graph, char* node_name);

void 
insert_link_between_two_nodes(node_t* n1, node_t* n2, char* to_intf, char* from_intf, unsigned int cost);

void
dump_node(node_t* node);

void
dump_intf(interface_t* intf);

#define GET_NODE(curr) (node_t*)((char*)curr - offsetof(node_t, graph_glue))

GLTHREAD_TO_STRUCT(graph_glue_to_node, node_t, graph_glue);
static inline int 
find_intf_available_slot(node_t *node) {
  for(int i=0; i<MAX_INTF_PER_NODE; i++) {
    if(node->intf[i] != NULL)
      continue;
    return i;
  }
  return -1;
}

static inline interface_t*
get_node_intf_by_name(node_t* node, char* local_if) {

 interface_t* intf;

 for(unsigned int i=0;i < MAX_INTF_PER_NODE; i++) {
   intf = node->intf[i];
   if (intf == NULL) return NULL;

   if(strncmp(intf->if_name, local_if, IF_NAME_SIZE) == 0)
     return intf;
 }
}

void dump_graph(graph_t* topo);
void dump_node(node_t* node);
void dump_intf(interface_t* intf);
#endif
