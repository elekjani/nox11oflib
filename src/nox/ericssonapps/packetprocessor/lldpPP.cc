#include "lldpPP.hh"

namespace ericsson {

#define LLDP_MESSENGER_NAME "lldp"

LldpPP::LldpPP(int type_id) : log("lldp-PP"), topo(),
    nodeToChId(topo), arcToPortId(topo), arcToDstPortId(topo), unknownNode(topo.addNode()) {
    this->type_id                 = type_id;
    this->messageTxInterval       = 5;
    this->messageTxHoldMultiplier = 2;
    this->notificationInterval    = 10;
    this->enabledPorts            = 3;
    this->disabledPorts           = 0;

    if (jsonDisp == NULL) {
        VLOG_WARN(log, "Connot find jsondispatcher.");
    } else {
        jsonDisp->registerHandler(LLDP_MESSENGER_NAME, "topo", boost::bind(&LldpPP::handleJsonTopo, this, _1));
    }
}

void LldpPP::join_handle(const datapathid &dpid, list<struct ofl_msg_processor_mod *> &proc_mod) {
    struct ofl_msg_processor_mod *pm = (struct ofl_msg_processor_mod *)malloc(sizeof(struct ofl_msg_processor_mod));

    struct lldp_pp_mod *lldp_pp_mod = (struct lldp_pp_mod *)malloc(sizeof(struct lldp_pp_mod));
    lldp_pp_mod->lldpMessageTxInterval       = htons(messageTxInterval);
    lldp_pp_mod->lldpMessageTxHoldMultiplier = htons(messageTxHoldMultiplier);
    lldp_pp_mod->lldpNotificationInterval    = htons(notificationInterval);

    lldp_pp_mod->pad[0] = 0;
    lldp_pp_mod->pad[1] = 0;

    lldp_pp_mod->enabledPorts  = htonl(enabledPorts); //portnumber 1 and 2
    lldp_pp_mod->disabledPorts = htonl(disabledPorts);

    pm->header.type = OFPT_PROCESSOR_MOD;
    pm->command = OFPPRC_ADD;
    pm->type    = type_id;
    pm->proc_id = next_proc_id();
    pm->data_length = sizeof(struct lldp_pp_mod);
    pm->data = (uint8_t *)lldp_pp_mod;

    proc_mod.push_back(pm);
    proc_id_registry[dpid].insert(pm->proc_id);

    return;
}

void LldpPP::barrier_handle(const datapathid &dpid, list<struct ofl_msg_flow_mod *> &flow_mod) {
    struct ofl_msg_flow_mod *fm = (struct ofl_msg_flow_mod *)malloc(sizeof(struct ofl_msg_flow_mod));

    struct ofl_instruction_goto_processor *goto_processor =
        (struct ofl_instruction_goto_processor *)malloc(sizeof(struct ofl_instruction_goto_processor));
    goto_processor->header.type = OFPIT_GOTO_PROCESSOR;
    goto_processor->processor_id = *(proc_id_registry[dpid].begin()); //in case of LLDP every dp has only one pp
    goto_processor->input_id = 1;

    struct ofl_instruction_header **insts = (struct ofl_instruction_header **)malloc(sizeof(struct ofl_instruction_header *));
    *insts = (struct ofl_instruction_header *)goto_processor;

    struct ofl_match_standard *match = (struct ofl_match_standard *)malloc(sizeof(struct ofl_match_standard));
    match->header.type = OFPMT_STANDARD;
    match->in_port = 1;
    match->wildcards = OFPFW_ALL;
    memset(match->dl_src_mask, 0xff, ETH_ADDR_LEN);
    memset(match->dl_dst_mask, 0xff, ETH_ADDR_LEN);
    match->dl_type       = ntohs(ethernet::LLDP);
    match->nw_src_mask   = 0xffffffff;
    match->nw_dst_mask   = 0xffffffff;
    match->metadata_mask = 0xffffffffffffffffULL;

    fm->header.type = OFPT_FLOW_MOD;
    fm->cookie = 0x00ULL;
    fm->cookie_mask = 0x00ULL;
    fm->table_id = 0; // use first table
    fm->command = OFPFC_ADD;
    fm->priority = OFP_DEFAULT_PRIORITY;
    fm->idle_timeout = 0;
    fm->hard_timeout = 0;
    fm->flags = ofd_flow_mod_flags();
    fm->match = (struct ofl_match_header *)match;
    fm->instructions_num = 1;
    fm->instructions = insts;

    flow_mod.push_back(fm);

    return;
}

void LldpPP::ctrl_handle(const datapathid &dpid, uint32_t proc_id, uint8_t *data, size_t data_length) {
    if(data_length > 0) {
        struct notifData n;
        n.status = *data >> 4;
        n.notifType = *data & 0x0F;
        n.length = ntohs(*((size_t *)(data + 1)));

        switch(n.notifType) {
            case CHASSIS_INFO: {
                struct chassisInfo *chassis = new chassisInfo();
                get_chassis_info(chassis, data);
                manageNodes(chassis);
                chIdToDpId[chassis->chassisId] = dpid;
                dpIdToChId[dpid] = chassis->chassisId;
                break;
            }
            case PORT_INFO: {
                struct portInfo *portinfo = new portInfo();
                get_port_info(portinfo, data);
                managePorts(portinfo);
                break;
            }
            case LINK_INFO: {
                struct linkInfo *linkinfo = new linkInfo();
                get_link_info(linkinfo, data);
                manageLinks(linkinfo);
                break;
            }
        }

    }

    return;
}


void LldpPP::dp_leaved(const datapathid &dpid, uint32_t proc_id) {
    proc_id_registry[dpid].erase(proc_id);

    ChIdToNodeMap::iterator chToNode_it = chIdToNode.find(dpIdToChId[dpid]);

    if(chToNode_it == chIdToNode.end()) {
        //TODO: error
        return;
    } else {
        sendJsonDpEvent(chToNode_it->second, false);

        for(ListDigraph::InArcIt inIt(topo, chToNode_it->second);
            inIt != INVALID;
            inIt = ListDigraph::InArcIt(topo, chToNode_it->second)) {
            topo.changeTarget(inIt, unknownNode);
        }

        for(ListDigraph::OutArcIt outIt(topo, chToNode_it->second); outIt != INVALID; ++outIt) {
            portIdToArc.erase(arcToPortId[outIt]);
            portIdToInfo.erase(arcToPortId[outIt]);
            topo.erase(outIt);
        }

        chIdToInfo.erase(chToNode_it->first);
        chIdToDpId.erase(chToNode_it->first);
        topo.erase(chToNode_it->second);
        chIdToNode.erase(chToNode_it->first);
        dpIdToChId.erase(dpid);
    }


    return;
}

auto_ptr<json_object> LldpPP::handleJsonTopo(const json_dict *json) {
    VLOG_DBG(log, "handleJsonTopo");

    json_object *msg = JsonDispatcher::jsonInit(LLDP_MESSENGER_NAME, "topo");
    json_dict *msgDict = static_cast<json_dict *>(msg->object);

    json_object *chs = new json_object(json_object::JSONT_ARRAY);
    json_array *chsArr = new json_array();
    chs->object = chsArr;

    for(ListDigraph::NodeIt it(topo); it != INVALID; ++it) {
        if(it != unknownNode)
            chsArr->push_back(chToJson(it));
    }

    msgDict->insert(make_pair("dps", chs));

    json_object *links = new json_object(json_object::JSONT_ARRAY);
    json_array *linksArr = new json_array();
    links->object = linksArr;

    for (ListDigraph::ArcIt it(topo); it != INVALID; ++it) {
        linksArr->push_back(linkToJson(it));
    }

    msgDict->insert(make_pair("links", links));

    return auto_ptr<json_object>(msg);
};

json_object *LldpPP::chToJson(ListDigraph::Node node) {
    json_object *obj = new json_object(json_object::JSONT_DICT);
    json_dict *objDict = new json_dict();
    obj->object = objDict;

    objDict->insert(make_pair("chid", JsonDispatcher::jsonString(nodeToChId[node])));

    return obj;
};

json_object *LldpPP::portToJson(PortIdPair &port) {
    json_object *obj = new json_object(json_object::JSONT_DICT);
    json_dict *objDict = new json_dict();
    obj->object = objDict;

    if(port.first == "" && port.second == "") {
        objDict->insert(make_pair("chid",   JsonDispatcher::jsonString("unknownNode")));
        objDict->insert(make_pair("portid", JsonDispatcher::jsonString("unknownPort")));
    }else {
        objDict->insert(make_pair("chid",   JsonDispatcher::jsonString(port.first)));
        objDict->insert(make_pair("portid", JsonDispatcher::jsonString(port.second)));
    }

    return obj;
}

json_object *LldpPP::linkToJson(ListDigraph::Arc arc) {
    json_object *obj = new json_object(json_object::JSONT_DICT);
    json_dict *objDict = new json_dict();
    obj->object = objDict;

    objDict->insert(make_pair("src", portToJson(arcToPortId[arc])));
    objDict->insert(make_pair("dst", portToJson(arcToDstPortId[arc])));

    return obj;
}

void LldpPP::get_chassis_info(struct chassisInfo *chassis, uint8_t *data) {
    chassis->notifData.status = *data >> 4;
    chassis->notifData.notifType = *data & 0x0F;
    data += sizeof(uint8_t);

    chassis->notifData.length = ntohl(*((size_t *)(data)));
    data += sizeof(size_t);

    chassis->chassisIdSubtype = *((enum LldpChassisIdSubtype*) data);
    chassis->chassisIdSubtype = (enum LldpChassisIdSubtype)htonl(chassis->chassisIdSubtype);
    data += sizeof(enum LldpChassisIdSubtype);

    chassis->chassisId.assign((char *)data);
    data += chassis->chassisId.length() + 1;

    if(*data != 0)
        chassis->sysName.assign((char *)data);

    data += chassis->sysName.length() + 1;

    if(*data != 0) {
        chassis->sysDesc.assign((char *)data);
    }

    data += chassis->sysName.length() + 1;

    return;
}

void LldpPP::get_port_info(struct portInfo *port, uint8_t *data) {
    port->notifData.status = *data >> 4;
    port->notifData.notifType = *data & 0x0F;
    data += sizeof(uint8_t);

    port->notifData.length = ntohl(*((size_t *)(data)));
    data += sizeof(size_t);

    get_chassis_info(&port->chassis, data);
    data += port->chassis.notifData.length;

    port->portNumber = ntohs(*((uint16_t *)data));
    data += sizeof(uint16_t);

    port->portIdSubtype = *((enum LldpPortIdSubtype*) data);
    port->portIdSubtype = (enum LldpPortIdSubtype)ntohl(port->portIdSubtype);
    data += sizeof(enum LldpPortIdSubtype);

    port->portId.assign((char *)data);
    data += port->portId.length() + 1;

    if(*data != 0) {
        port->portDesc.assign((char *)data);
    }

    data += port->portDesc.length() + 1;

    return;
}

void LldpPP::get_link_info(struct linkInfo *link, uint8_t *data) {
    link->notifData.status = *data >> 4;
    link->notifData.notifType = *data & 0x0F;
    data += sizeof(uint8_t);

    link->notifData.length = ntohl(*((size_t *)(data)));
    data += sizeof(size_t);

    get_port_info(&link->srcPort, data);
    data += link->srcPort.notifData.length;

    get_port_info(&link->dstPort, data);
    data += link->dstPort.notifData.length;

    return;
}

ListDigraph::Node LldpPP::manageNodes(struct chassisInfo *chassis) {
    ChIdToNodeMap::iterator chToNode_it = chIdToNode.find(chassis->chassisId);
    ChIdToInfoMap::iterator chToInfo_it = chIdToInfo.find(chassis->chassisId);

    ListDigraph::Node node;

    switch(chassis->notifData.status) {
        case LLDP_ADDED: {
            if(chToNode_it == chIdToNode.end()) {
                node         = topo.addNode();
                chIdToNode[chassis->chassisId] = node;
                nodeToChId[node]               = chassis->chassisId;
                sendJsonDpEvent(node, true);
            } else {
                node = chToNode_it->second;
            }

            if(chToInfo_it == chIdToInfo.end()) {
                chIdToInfo[chassis->chassisId] = *chassis;
            }

            break;
        }
        case LLDP_CHANGED: {
            if(chToInfo_it != chIdToInfo.end()) {
                chIdToInfo.erase(chToInfo_it);
                chIdToInfo[chassis->chassisId] = *chassis;
            }

            if(chToNode_it == chIdToNode.end()) {
                //TODO: error
                node = INVALID;
            } else {
                node = chToNode_it->second;
            }

            break;
        }
        case LLDP_DELETED: {

            if(chToNode_it != chIdToNode.end()) {
                sendJsonDpEvent(chToNode_it->second, false);
                topo.erase(chToNode_it->second);
                chIdToNode.erase(chToNode_it);
            }

            if(chToInfo_it != chIdToInfo.end()) {
                chIdToInfo.erase(chToInfo_it);
            }

            node = INVALID;

            break;
        }
        case LLDP_NONE:
        default: {
            if(chToNode_it != chIdToNode.end()) {
                node = chToNode_it->second;
            } else {
                node = INVALID;
            }

            break;
        }
    }

    if(node == INVALID)
        VLOG_ERR(log, "managenodes return INVALID");

    return node;
}

ListDigraph::Arc LldpPP::managePorts(struct portInfo *port) {
    ChIdToNodeMap::iterator chToNode_it = chIdToNode.find(port->chassis.chassisId);
    ChIdToInfoMap::iterator chToInfo_it = chIdToInfo.find(port->chassis.chassisId);

    PortIdPair portPair(port->chassis.chassisId, port->portId);
    PortIdToArcMap::iterator portToArc_it   = portIdToArc.find(portPair);
    PortIdToInfoMap::iterator portToInfo_it = portIdToInfo.find(portPair);

    ListDigraph::Arc arc;
    ListDigraph::Node sourceNode = manageNodes(&port->chassis);

    switch(port->notifData.status) {
        case LLDP_ADDED: {
            if(portToArc_it == portIdToArc.end()) {
                arc = topo.addArc(sourceNode, unknownNode);
                portIdToArc[portPair] = arc;
                arcToPortId[arc] = portPair;
            } else {
                arc = portToArc_it->second;
            }

            if(portToInfo_it == portIdToInfo.end()) {
                portIdToInfo[portPair] = *port;
            }

            break;
        }
        case LLDP_CHANGED: {
            if(portToInfo_it != portIdToInfo.end()) {
                portIdToInfo.erase(portToInfo_it);
                portIdToInfo[portPair] = *port;
            }

            if(portToArc_it == portIdToArc.end()) {
                arc = INVALID;
            } else {
                arc = portToArc_it->second;
            }

            break;
        }
        case LLDP_DELETED: {

            if(portToArc_it != portIdToArc.end()) {
                sendJsonLinkEvent(portToArc_it->second, false);
                topo.erase(portToArc_it->second);
                portIdToArc.erase(portToArc_it);
            }

            if(portToInfo_it != portIdToInfo.end()) {
                portIdToInfo.erase(portToInfo_it);
            }

            arc = INVALID;

            break;
        }
        case LLDP_NONE:
        default: {
            if(portToArc_it != portIdToArc.end()) {
                arc = portToArc_it->second;
            } else {
                arc = INVALID;
            }

            break;
        }
    }

    if(arc == INVALID)
        VLOG_ERR(log, "manageports return INVALID");

    return arc;
}

void LldpPP::manageLinks(struct linkInfo *link) {
    ListDigraph::Arc srcArc = managePorts(&link->srcPort);
    ListDigraph::Arc dstArc = managePorts(&link->dstPort);

    if(link->notifData.status == LLDP_DELETED && srcArc != INVALID) {
        sendJsonLinkEvent(srcArc, false);
        topo.changeTarget(srcArc, unknownNode);
        arcToDstPortId[srcArc] = PortIdPair("unknownNode", "unknownPort");
    }else if(srcArc != INVALID && dstArc != INVALID) {
        topo.changeTarget(srcArc, topo.source(dstArc));
        arcToDstPortId[srcArc] = PortIdPair(link->dstPort.chassis.chassisId, link->dstPort.portId);
        sendJsonLinkEvent(srcArc, true);
    }

    return;
}

void LldpPP::sendJsonDpEvent(ListDigraph::Node node, bool add) {
    if(!jsonDisp->isConnected()) {
        return;
    }

    json_object *msg = JsonDispatcher::jsonInit(LLDP_MESSENGER_NAME, "dp");
    json_dict *msgDict = static_cast<json_dict *>(msg->object);

    msgDict->insert(make_pair("dp", chToJson(node)));
    msgDict->insert(make_pair("event", JsonDispatcher::jsonString(add ? "join" : "leave")));

    jsonDisp->send(auto_ptr<json_object>(msg));

    return;
}

void LldpPP::sendJsonLinkEvent(ListDigraph::Arc arc, bool add) {
    if(!jsonDisp->isConnected()) {
        return;
    }

    json_object *msg = JsonDispatcher::jsonInit(LLDP_MESSENGER_NAME, "link");
    json_dict *msgDict = static_cast<json_dict *>(msg->object);

    msgDict->insert(make_pair("link", linkToJson(arc)));
    msgDict->insert(make_pair("event",JsonDispatcher::jsonString(add ? "add" : "remove")));

    jsonDisp->send(auto_ptr<json_object>(msg));

    return;
}

} //namespace ericsson
