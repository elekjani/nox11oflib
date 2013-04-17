#include "packetprocessor.hh"

namespace ericsson {

#define MESSENGER_NAME "packetprocessor"

    void Packetprocessor::configure(const Configuration* conf) {
        VLOG_DBG(log, "invoked configure");
        register_handler<Datapath_join_event>(boost::bind(&Packetprocessor::join_handle,this,_1));
        register_handler<Datapath_leave_event>(boost::bind(&Packetprocessor::leave_handle,this,_1));
        register_handler(Ofp_msg_event::get_name(OFPT_PROCESSOR_CTRL), boost::bind(&Packetprocessor::ctrl_handle, this, _1));
        register_handler(Ofp_msg_event::get_name(OFPT_BARRIER_REPLY), boost::bind(&Packetprocessor::barrier_handle, this, _1));
        register_handler(Ofp_msg_event::get_name(OFPT_PACKET_IN), boost::bind(&Packetprocessor::packet_in_handle, this, _1));
    }

    void Packetprocessor::install() {
        jsonDisp = assert_cast<JsonDispatcher *>(ctxt->get_by_name("jsondispatcher"));
        if (jsonDisp == NULL) {
            VLOG_WARN(log, "Cannot find jsondispatcher.");
        }else{
            jsonDisp->registerHandler(MESSENGER_NAME, "test", boost::bind(&Packetprocessor::handleJsonTest, this, _1));
        }

        BasePP::jsonDisp = jsonDisp;

        localPP_registry[LLDP_TYPE]          = new LldpPP(LLDP_TYPE);
        localPP_registry[LLDP_TYPE]->enabled = true;
        localPP_registry[GRE_TYPE]           = new GrePP(GRE_TYPE);
        localPP_registry[GRE_TYPE]->enabled  = false;

        return;
    }

    Disposition Packetprocessor::ctrl_handle(const Event& e) {
        const Ofp_msg_event& pi = assert_cast<const Ofp_msg_event&>(e);
        struct ofl_msg_header *in = **pi.msg;

        struct ofl_msg_processor_ctrl *proc_ctrl = (struct ofl_msg_processor_ctrl*)in;
        localPP_registry[proc_ctrl->type]->ctrl_handle(pi.dpid, proc_ctrl->proc_id, proc_ctrl->data, proc_ctrl->data_length);

        return CONTINUE;
    }

    Disposition Packetprocessor::barrier_handle(const Event& e) {
        VLOG_DBG(log,"barrier reply message arrived");
        const Ofp_msg_event& pi = assert_cast<const Ofp_msg_event&>(e);

        for(map<int, BasePP*>::iterator map_it=localPP_registry.begin(); map_it!=localPP_registry.end(); map_it++) {
            if(map_it->second->enabled) {
                list<struct ofl_msg_flow_mod*> flow_mod;
                map_it->second->barrier_handle(pi.dpid, flow_mod);
                for(list<struct ofl_msg_flow_mod*>::iterator list_it=flow_mod.begin(); list_it!=flow_mod.end(); list_it++) {
                    int ret = send_openflow_msg(pi.dpid, (struct ofl_msg_header *)*list_it, 0, true);
                    if(ret) {
                        VLOG_ERR(log, "send_openflow_msg error");
                    }
                    ofl_msg_free((struct ofl_msg_header *)*list_it,NULL);
                }
            }
        }

        return CONTINUE;
    }

    Disposition Packetprocessor::join_handle(const Event& e) {
        VLOG_DBG(log,"datapath joined");
        const Datapath_join_event& pi = assert_cast<const Datapath_join_event&>(e);

        for(map<int, BasePP*>::iterator map_it=localPP_registry.begin(); map_it!=localPP_registry.end(); map_it++) {
            if(map_it->second->enabled) {
                list<struct ofl_msg_processor_mod*> proc_mod;
                map_it->second->join_handle(pi.dpid, proc_mod);
                for(list<struct ofl_msg_processor_mod*>::iterator list_it=proc_mod.begin(); list_it!=proc_mod.end(); list_it++) {
                    VLOG_DBG(log, "send_openflow_msg");
                    int ret = send_openflow_msg(pi.dpid, (struct ofl_msg_header *)*list_it, 0, true);
                    if(ret) {
                        VLOG_ERR(log, "send_openflow_msg error");
                    }

                    struct ppSkeleton skel = {(*list_it)->proc_id,(*list_it)->type};
                    networkPP_registry[pi.dpid].push_back(skel);

                    ofl_msg_free((struct ofl_msg_header *)*list_it,NULL);
                }
            }
        }

        int ret = send_openflow_barrier_request(pi.dpid, true);
        if(ret) {
            VLOG_ERR(log, "send_barrier_request error");
        }

        return CONTINUE;
    }

    Disposition Packetprocessor::leave_handle(const Event& e) {
        VLOG_DBG(log, "datapath leaved");
        const Datapath_leave_event& pi = assert_cast<const Datapath_leave_event&>(e);

        map<datapathid, list<ppSkeleton> >::iterator skel_list = networkPP_registry.find(pi.datapath_id);
        for(list<ppSkeleton>::iterator it=skel_list->second.begin(); it!=skel_list->second.end(); it++) {
            localPP_registry[it->type_id]->dp_leaved(pi.datapath_id, it->proc_id);
        }

        networkPP_registry.erase(pi.datapath_id);
        return CONTINUE;
    }

    Disposition Packetprocessor::packet_in_handle(const Event& e) {

        return CONTINUE;
    }

    REGISTER_COMPONENT(container::Simple_component_factory<Packetprocessor>, Packetprocessor);
} // ericsson namespace
