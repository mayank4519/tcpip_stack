#include "graph.h"
#include <stdio.h>
#include "CommandParser/libcli.h"
#include "CommandParser/cmdtlv.h"
#include "cmdcodes.h"

extern graph_t *topo;

/* Display functions when user presses ?*/
void
display_graph_nodes(param_t *param, ser_buff_t *tlv_buf){

    node_t *node;
    gl_thread_t *curr;

    ITERATE_GLTHREAD_BEGIN(&topo->node_list, curr){

        node = graph_glue_to_node(curr);
        printf("%s\n", node->node_name);
    } ITERATE_GLTHREAD_END(&topo->node_list, curr);
}

/*General Validations*/
int
validate_node_extistence(char *node_name){

    node_t *node = get_node_by_node_name(topo, node_name);
    if(node)
        return VALIDATION_SUCCESS;
    printf("Error : Node %s do not exist\n", node_name);
    return VALIDATION_FAILED;
}

/*Generic Topology Commands*/
static int
show_nw_topology_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    int CMDCODE = -1;
    CMDCODE = EXTRACT_CMD_CODE(tlv_buf);

    switch(CMDCODE){

        case CMDCODE_SHOW_NW_TOPOLOGY:
            dump_nw_graph(topo);
            break;
        default:
            ;
    }
}

typedef struct arp_table_ arp_table_t;

extern void
dump_arp_table(arp_table_t *arp_table);

static int
show_arp_handler(param_t *param, ser_buff_t *tlv_buf,
                    op_mode enable_or_disable){

    node_t *node;
    char *node_name;
    tlv_struct_t *tlv = NULL;

    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;

    }TLV_LOOP_END;

    node = get_node_by_node_name(topo, node_name);
    dump_arp_table(NODE_ARP_TABLE(node));
    return 0;
}

extern void
send_arp_broadcast_request(node_t *node,
                           interface_t *oif,
                           char *ip_addr);

static int
arp_handler(param_t *param, ser_buff_t *tlv_buf,
                op_mode enable_or_disable){

    node_t *node;
    char *node_name;
    char *ip_addr;
    tlv_struct_t *tlv = NULL;

    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "ip-address", strlen("ip-address")) ==0)
            ip_addr = tlv->value;
    } TLV_LOOP_END;

    node = get_node_by_node_name(topo, node_name);
    send_arp_broadcast_request(node, NULL, ip_addr);
    return 0;
}

typedef struct mac_table_ mac_table_t;
extern void
dump_mac_table(mac_table_t *mac_table);
static int
show_mac_handler(param_t *param, ser_buff_t *tlv_buf,
                    op_mode enable_or_disable){

    node_t *node;
    char *node_name;
    tlv_struct_t *tlv = NULL;

    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;

    }TLV_LOOP_END;

    node = get_node_by_node_name(topo, node_name);
    dump_mac_table(NODE_MAC_TABLE(node));
    return 0;

}

extern void
interface_set_l2_mode(node_t* node,
                     interface_t* interface, char* l2mode);

int
validate_l2_mode_value(char *l2_mode_value){

    if((strncmp(l2_mode_value, "access", strlen("access")) == 0) ||
        (strncmp(l2_mode_value, "trunk", strlen("trunk")) == 0))
        return VALIDATION_SUCCESS;
    return VALIDATION_FAILED;
}

static int
intf_config_handler(param_t *param, ser_buff_t *tlv_buf,
                    op_mode enable_or_disable){

   char *node_name;
   char *intf_name;
   unsigned int vlan_id;
   char *l2_mode_option;
   int CMDCODE;
   tlv_struct_t *tlv = NULL;
   node_t *node;
   interface_t *interface;

   CMDCODE = EXTRACT_CMD_CODE(tlv_buf);

    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if     (strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "if-name", strlen("if-name")) ==0)
            intf_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "vlan-id", strlen("vlan-d")) ==0)
            vlan_id = atoi(tlv->value);
        else if(strncmp(tlv->leaf_id, "l2-mode-val", strlen("l2-mode-val")) == 0)
            l2_mode_option = tlv->value;
        else
            assert(0);
    } TLV_LOOP_END;

    node = get_node_by_node_name(topo, node_name);
    interface = get_node_intf_by_name(node, intf_name);

    if(!interface){
        printf("Error : Interface %s do not exist\n", interface->if_name);
        return -1;
    }
    switch(CMDCODE){
        case CMDCODE_INTF_CONFIG_L2_MODE:
            switch(enable_or_disable){
                case CONFIG_ENABLE:
                    interface_set_l2_mode(node, interface, l2_mode_option);
                    break;
                default:
                    ;
            }
            break;
        case CMDCODE_INTF_CONFIG_VLAN:
            switch(enable_or_disable){
                default:
                    ;
            }
            break;
         default:
            ;
    }
    return 0;
}



void
nw_init_cli(){

    init_libcli();

    param_t *show   = libcli_get_show_hook();
    param_t *debug  = libcli_get_debug_hook();
    param_t *config = libcli_get_config_hook();
    param_t *run    = libcli_get_run_hook();
    param_t *debug_show = libcli_get_debug_show_hook();
    param_t *root = libcli_get_root();

    {
        /*show topology*/
         static param_t topology;
         init_param(&topology, CMD, "topology", show_nw_topology_handler, 0, INVALID, 0, "Dump Complete Network Topology");
         libcli_register_param(show, &topology);
         set_param_cmd_code(&topology, CMDCODE_SHOW_NW_TOPOLOGY);    

	 {
            /*show node*/
             static param_t node;
             init_param(&node, CMD, "node", 0, 0, INVALID, 0, "\"node\" keyword");
             libcli_register_param(show, &node);
             libcli_register_display_callback(&node, display_graph_nodes);
             {
                /*show node <node-name>*/
                 static param_t node_name;
                 init_param(&node_name, LEAF, 0, 0, validate_node_extistence, STRING, "node-name", "Node Name");
                 libcli_register_param(&node, &node_name);
                 {
                    /*show node <node-name> arp*/
                    static param_t arp;
                    init_param(&arp, CMD, "arp", show_arp_handler, 0, INVALID, 0, "\"arp\" keyword");
                    libcli_register_param(&node_name, &arp);
                    set_param_cmd_code(&arp, CMDCODE_SHOW_NODE_ARP_TABLE);
                 }
		 {
                    /*show node <node-name> mac*/
                    static param_t mac;
                    init_param(&mac, CMD, "mac", show_mac_handler, 0, INVALID, 0, "Dump Mac table");
                    libcli_register_param(&node_name, &mac);
                    set_param_cmd_code(&mac, CMDCODE_SHOW_NODE_MAC_TABLE);
                 }
             }
         }
    }
   
    {
      /*run node*/
        static param_t node;
        init_param(&node, CMD, "node", 0, 0, INVALID, 0, "\"node\" keyword");
        libcli_register_param(run, &node);
        libcli_register_display_callback(&node, display_graph_nodes);
	{
	    /*run node <node-name>*/
            static param_t node_name;
            init_param(&node_name, LEAF, 0, 0, validate_node_extistence, STRING, "node-name", "Node Name");
	    libcli_register_param(&node, &node_name);
	    {
	    }
            /*run node <node-name> resolve-arp*/
            static param_t resolve_arp;
            init_param(&resolve_arp, CMD, "resolve-arp", 0, 0, INVALID, 0, "Resolve ARP");
            libcli_register_param(&node_name, &resolve_arp);
            {
                /*run node <node-name> resolve-arp <ip-address>*/
                static param_t ip_addr;
                init_param(&ip_addr, LEAF, 0, arp_handler, 0, IPV4, "ip-address", "Nbr IPv4 Address");
                libcli_register_param(&resolve_arp, &ip_addr);
                set_param_cmd_code(&ip_addr, CMDCODE_RUN_ARP);
            }
	 }
     }

    /*config node*/
    {
      static param_t node;
      init_param(&node, CMD, "node", 0, 0, INVALID, 0, "\"node\" keyword");
      libcli_register_param(config, &node);
      libcli_register_display_callback(&node, display_graph_nodes);
      {
        /*config node <node-name>*/
        static param_t node_name;
        init_param(&node_name, LEAF, 0, 0, validate_node_extistence, STRING, "node-name", "Node Name");
        libcli_register_param(&node, &node_name);
        {
            /*config node <node-name> interface*/
            static param_t interface;
            init_param(&interface, CMD, "interface", 0, 0, INVALID, 0, "\"interface\" keyword");
            libcli_register_param(&node_name, &interface);
            {
                /*config node <node-name> interface <if-name>*/
                static param_t if_name;
                init_param(&if_name, LEAF, 0, 0, 0, STRING, "if-name", "Interface Name");
                libcli_register_param(&interface, &if_name);
                {
                    /*config node <node-name> interface <if-name> l2mode*/
                    static param_t l2_mode;
                    init_param(&l2_mode, CMD, "l2mode", 0, 0, INVALID, 0, "\"l2mode\" keyword");
                    libcli_register_param(&if_name, &l2_mode);
                    {
                        /*config node <node-name> interface <if-name> l2mode <access|trunk>*/
                        static param_t l2_mode_val;
                        init_param(&l2_mode_val, LEAF, 0, intf_config_handler, validate_l2_mode_value,  STRING, "l2-mode-val", "access|trunk");
                        libcli_register_param(&l2_mode, &l2_mode_val);
                        set_param_cmd_code(&l2_mode_val, CMDCODE_INTF_CONFIG_L2_MODE);
                    }
                }
            }

        }
      support_cmd_negation(&node_name);
      }
    } 
    support_cmd_negation(config);
    /*Do not Add any param here*/
}
