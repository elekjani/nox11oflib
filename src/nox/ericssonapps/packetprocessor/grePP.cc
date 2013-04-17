#include "grePP.hh"

namespace ericsson {

    GrePP::GrePP(int type_id) : log("gre-PP") {
        this->type_id = type_id;
        local_dpid      = 0xa1;
        local_ip        = IP_ADDR(11,0,0,1);
        local_tunnel_ip = IP_ADDR(10,0,0,1);
        local_interface = 2;

        remote_dpid      = 0xa2;
        remote_ip        = IP_ADDR(11,0,0,2);
        remote_tunnel_ip = IP_ADDR(10,0,0,2);
        remote_interface = 1;
    };

    void GrePP::join_handle(const datapathid &dpid, list<struct ofl_msg_processor_mod*> &proc_mod) {
        struct ofl_msg_processor_mod *pm = (struct ofl_msg_processor_mod*)malloc(sizeof(struct ofl_msg_processor_mod));

        struct gre_pp_mod *gre_pp_mod = (struct gre_pp_mod*)malloc(sizeof(struct gre_pp_mod));
        if (dpid.as_host() == local_dpid) {
            gre_pp_mod->local_ip         = htonl(local_ip);
            gre_pp_mod->remote_ip        = htonl(remote_ip);
            gre_pp_mod->local_tunnel_ip  = htonl(local_tunnel_ip);
            gre_pp_mod->remote_tunnel_ip = htonl(remote_tunnel_ip);
            gre_pp_mod->interface        = htonl(local_interface);
        }else if (dpid.as_host() == remote_dpid) {
            gre_pp_mod->remote_ip        = htonl(local_ip);
            gre_pp_mod->local_ip         = htonl(remote_ip);
            gre_pp_mod->local_tunnel_ip  = htonl(local_tunnel_ip);
            gre_pp_mod->remote_tunnel_ip = htonl(remote_tunnel_ip);
            gre_pp_mod->interface        = htonl(remote_interface);
        }else {
            free(gre_pp_mod);
            return;
        }

        pm->header.type = OFPT_PROCESSOR_MOD;
        pm->type    = type_id;
        pm->proc_id = next_proc_id();
        pm->command = OFPPRC_ADD;
        pm->data_length = sizeof(struct gre_pp_mod);
        pm->data = (uint8_t*)gre_pp_mod;

        proc_mod.push_back(pm);
        proc_id_registry[dpid].insert(pm->proc_id);

        return;
   }

    void GrePP::barrier_handle(const datapathid &dpid, list<struct ofl_msg_flow_mod*> &flow_mod) {
        struct ofl_instruction_goto_processor *action1 = (struct ofl_instruction_goto_processor*)malloc(sizeof(struct ofl_instruction_goto_processor));
        action1->header.type = OFPIT_GOTO_PROCESSOR;
        action1->processor_id = *(proc_id_registry[dpid].begin()); //in case of LLDP every dp has only one pp
        action1->input_id = 1;

        struct ofl_instruction_header **insts1 = (struct ofl_instruction_header**)malloc(sizeof(struct ofl_instruction_header*));
        *insts1 = (struct ofl_instruction_header *)action1;

        struct ofl_match_standard *match1 = (struct ofl_match_standard*)malloc(sizeof(struct ofl_match_standard));
        match1->header.type = OFPMT_STANDARD;
        match1->in_port = 1;
        match1->wildcards = (OFPFW_ALL & ~OFPFW_DL_TYPE);
        memset(match1->dl_src_mask, 0xff, ETH_ADDR_LEN);
        memset(match1->dl_dst_mask, 0xff, ETH_ADDR_LEN);
        match1->dl_type       = ETH_TYPE_IP;
        match1->nw_dst        = (dpid.as_host() == remote_dpid) ? local_tunnel_ip : remote_tunnel_ip;
        match1->nw_dst        = htonl(match1->nw_dst);
        match1->nw_src_mask   = 0xffffffff;
        match1->nw_dst_mask   = 0x00000000;
        match1->metadata_mask = 0xffffffffffffffffULL;

        struct ofl_msg_flow_mod *fm = (struct ofl_msg_flow_mod*)malloc(sizeof(struct ofl_msg_flow_mod));
        fm->header.type = OFPT_FLOW_MOD;
        fm->cookie = 0x00ULL;
        fm->cookie_mask = 0x00ULL;
        fm->table_id = 0; // use first table
        fm->command = OFPFC_ADD;
        fm->priority = OFP_DEFAULT_PRIORITY;
        fm->idle_timeout = 0;
        fm->hard_timeout = 0;
        fm->flags = ofd_flow_mod_flags();
        fm->match = (struct ofl_match_header *)match1;
        fm->instructions_num = 1;
        fm->instructions = insts1;

        flow_mod.push_back(fm);

        struct ofl_instruction_goto_processor *action2 = (struct ofl_instruction_goto_processor*)malloc(sizeof(struct ofl_instruction_goto_processor));
        memcpy(action2, action1, sizeof(struct ofl_instruction_goto_processor));
        action2->input_id = 2;

        struct ofl_instruction_header **insts2 = (struct ofl_instruction_header**)malloc(sizeof(struct ofl_instruction_header*));
        *insts2 = (struct ofl_instruction_header *)action2;

        struct ofl_match_standard *match2 = (struct ofl_match_standard*)malloc(sizeof(struct ofl_match_standard));
        memcpy(match2, match1, sizeof(struct ofl_match_standard));
        match2->wildcards      &= ~OFPFMF_NW_PROTO;
        match2->nw_proto       = IP_TYPE_GRE;
        match2->nw_dst         = (dpid.as_host() == remote_dpid) ? remote_ip : local_ip;
        match2->nw_dst         = htonl(match2->nw_dst);


        fm = (struct ofl_msg_flow_mod*)malloc(sizeof(struct ofl_msg_flow_mod));
        memcpy(fm,flow_mod.front(),sizeof(struct ofl_msg_flow_mod));

        fm->match = (struct ofl_match_header *)match2;
        fm->instructions = insts2;

        flow_mod.push_back(fm);

        return;

    }

    void GrePP::dp_leaved(const datapathid &dpid, uint32_t proc_id) {
        proc_id_registry[dpid].erase(proc_id);

        return;
    }

}
