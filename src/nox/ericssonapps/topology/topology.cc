#include "topology.hh"

#include <boost/bind.hpp>

#include <lemon/lgf_writer.h> // for debugging

#include "nox.hh"
#include "assert.hh"

#include "openflow/openflow.h"

#include "../../oflib/ofl-messages.h"
#include "../../oflib/ofl-structs.h"

//events
#include "datapath-join.hh"
#include "datapath-leave.hh"
#include "netapps/discovery/link-event.hh"
#include "topologychangedevent.hh"


using namespace std;
using namespace vigil;
using namespace vigil::container;

/* @@ZED@@ find proper .h file with min */
#define MIN(a,b) a < b ? a : b

/* @@ZED@@ Listen to port change events */
/* @@ZED@@ link remove event after dpleave should not result in warning */

namespace ericsson {

#define MESSENGER_NAME "topology"

const Component_name
Topology::component_name() {
	return std::string("topology");
}

Topology::Topology(const Context *c, const json_object *json) : Component(c),
		topo(), nodeToDpid(topo), arcToLink(topo),
		dpidToNode(), linkToArc(),
		dpidToPorts(), portToLinks(),
		portToProps(), linkToProps(),
		jsonDisp(NULL),
		lg("topology")
	{ };

void
Topology::configure(const Configuration *conf) { };

void
Topology::install() {
	jsonDisp = assert_cast<JsonDispatcher *>(ctxt->get_by_name("jsondispatcher"));
	if (jsonDisp == NULL) {
		lg.warn("Cannot find jsondispatcher.");
	} else {
		jsonDisp->registerHandler(MESSENGER_NAME, "topo", boost::bind(&Topology::handleJsonTopo, this, _1));
		jsonDisp->registerHandler(MESSENGER_NAME, "dump", boost::bind(&Topology::handleJsonDump, this, _1));
	}

	register_event(TopologyChangedEvent::static_get_name());

	register_handler(Datapath_join_event::static_get_name(), boost::bind(&Topology::handleDatapathJoin, this, _1));
	register_handler(Datapath_leave_event::static_get_name(), boost::bind(&Topology::handleDatapathLeave, this, _1));
	register_handler(Link_event::static_get_name(), boost::bind(&Topology::handleLinkChange, this, _1));
};



Disposition
Topology::handleDatapathJoin(const Event& e) {
	const Datapath_join_event& dpj = assert_cast<const Datapath_join_event&>(e);
	struct ofl_msg_features_reply *feat = (struct ofl_msg_features_reply *)**dpj.msg;
	const datapathid dpid = datapathid::from_host(feat->datapath_id);

	lg.dbg("handleDatapathJoin: %"PRIx64".", dpid.as_host());

	if (dpidToNode.find(dpid) != dpidToNode.end()) {
		lg.emer("Datapath (%"PRIx64") is joining, but is already in the topology database.", dpid.as_host());
		return CONTINUE;
	}

	ListDigraph::Node node = topo.addNode();
	nodeToDpid[node] = dpid;
	dpidToNode[dpid] = node;

	for (size_t i=0; i < feat->ports_num; i++) {
		if (feat->ports[i]->port_no > OFPP_MAX) {
			lg.dbg("skipping special port %"PRIu32".", feat->ports[i]->port_no);
			continue;
		}

		DpPort port = DpPort(dpid, feat->ports[i]->port_no);

		if (portToProps.find(port) != portToProps.end()) {
			lg.warn("Port (%"PRIx64"/%"PRIu32") is added, but is already in the topology database.", port.dpid.as_host(), port.port);
			portToProps.erase(port);
		}

		uint32_t port_speed = feat->ports[i]->curr_speed == 0 ||
				              feat->ports[i]->curr_speed == 0xffffffff
									? DEFAULT_PORT_SPEED
									: feat->ports[i]->curr_speed;

		/* @@ZED@@ Check if port is administratively or physically down */

		dpidToPorts.insert(make_pair(dpid, port));
		portToProps[port] = DpPortProps(port_speed);

		lg.dbg("Port (%"PRIx64"/%"PRIu32") is added with %"PRIu32"kbps speed.", port.dpid.as_host(), port.port, port_speed);
	}

	nox::post_event(new TopologyChangedEvent());
	sendJsonDpEvent(dpid, true);

	return CONTINUE;
}

Disposition
Topology::handleDatapathLeave(const Event& e) {
	const Datapath_leave_event& dpl = assert_cast<const Datapath_leave_event&>(e);

	const datapathid& dpid = dpl.datapath_id;

	lg.dbg("handleDatapathLeave %"PRIx64".", dpid.as_host());

	DpidToNodeMap::iterator dpidToNodeIter = dpidToNode.find(dpid);
	if (dpidToNodeIter == dpidToNode.end()) {
		lg.warn("Datapath (%"PRIx64") is leaving, but is not in the topology database.", dpid.as_host());
		return CONTINUE;
	}

	/* go through ports of the datapath */
	for (DpidToPortsMap::iterator pIt = dpidToPorts.lower_bound(dpid); pIt != dpidToPorts.upper_bound(dpid); ) {

		PortToLinksMap::iterator it = portToLinks.lower_bound(pIt->second);
		while(it != portToLinks.upper_bound(pIt->second)) {
			handleLinkRemove(it->second);
			it = portToLinks.lower_bound(pIt->second);
		}

		/* remove port props */
		portToProps.erase(pIt->second);

		/* remove dpport */
		dpidToPorts.erase(pIt++);
	}

	/* remove node */
	topo.erase(dpidToNodeIter->second);

	/* remove dp */
	dpidToNode.erase(dpidToNodeIter);

	nox::post_event(new TopologyChangedEvent());
	sendJsonDpEvent(dpid, false);

	return CONTINUE;
}

void
Topology::handleLinkAdd(const DpLink& link) {
	DpidToNodeMap::iterator dpidToNodeIterSrc = dpidToNode.find(link.src.dpid);
	if (dpidToNodeIterSrc == dpidToNode.end()) {
		lg.warn("Link to be added has no associated src node in topology database."); /* @@ZED@@ print dp info */
		return;
	}
	DpidToNodeMap::iterator dpidToNodeIterDst = dpidToNode.find(link.dst.dpid);
	if (dpidToNodeIterDst == dpidToNode.end()) {
		lg.warn("Link to be added has no associated dst node in topology database."); /* @@ZED@@ print dp info */
		return;
	}

	PortToPropsMap::iterator portToPropsIterSrc = portToProps.find(link.src);
	if (portToPropsIterSrc == portToProps.end()) {
		lg.warn("Link to be added has no associated src port in topology database."); /* @@ZED@@ print port info */
		return;
	}
	PortToPropsMap::iterator portToPropsIterDst = portToProps.find(link.dst);
	if (portToPropsIterDst == portToProps.end()) {
		lg.warn("Link to be added has no associated dst port in topology database."); /* @@ZED@@ print port info */
		return;
	}

	LinkToPropsMap::iterator linkToPropsIter = linkToProps.find(link);
	if (linkToPropsIter != linkToProps.end()) {
		lg.warn("Link to be added already exists in the topology database."); /* @@ZED@@ print link info */
	}

	/* create arc */
	ListDigraph::Arc arc = topo.addArc(dpidToNodeIterSrc->second, dpidToNodeIterDst->second);
	arcToLink[arc] = link;
	linkToArc[link] = arc;

	/* add props */
	DpLinkProps dpLinkProps = DpLinkProps(MIN(portToPropsIterSrc->second.speed, portToPropsIterDst->second.speed));
	linkToProps[link] = dpLinkProps;

	/* add to ports */
	portToLinks.insert(make_pair(link.src, link));
	portToLinks.insert(make_pair(link.dst, link));

	nox::post_event(new TopologyChangedEvent());
	sendJsonLinkEvent(link, true);
}

void
Topology::handleLinkRemove(const DpLink& link) {
	LinkToPropsMap::iterator linkToPropsIter = linkToProps.find(link);
	if (linkToPropsIter == linkToProps.end()) {
		lg.warn("Link to be removed is not in the topology database."); /* @@ZED@@ print link info */
		return;
	}

	LinkToArcMap::iterator linkToArcIter = linkToArc.find(link);
	if (linkToArcIter == linkToArc.end()) {
		lg.warn("Link to be removed is not in the topology database."); /* @@ZED@@ print link info */
		return;
	}

	topo.erase(linkToArcIter->second);
	/* @@ZED@@ make sure arc is removed from arcToLink */

	/* remove arc*/
	linkToArc.erase(linkToArcIter);

	/* remove link props */
	linkToProps.erase(linkToPropsIter);

	/* remove link from ports (src) */
	for(PortToLinksMap::iterator it = portToLinks.lower_bound(link.src); it != portToLinks.upper_bound(link.src); ) {
		if (it->second == link) {
			portToLinks.erase(it++);
		} else {
			++it;
		}
	}

	/* remove link from ports (dst) */
	for(PortToLinksMap::iterator it = portToLinks.lower_bound(link.dst); it != portToLinks.upper_bound(link.dst); ) {
		if (it->second == link) {
			portToLinks.erase(it++);
		} else {
			++it;
		}
	}

	nox::post_event(new TopologyChangedEvent());
	sendJsonLinkEvent(link, false);
}


Disposition
Topology::handleLinkChange(const Event& e) {
	const Link_event& le = assert_cast<const Link_event&>(e);

	lg.dbg("handleLinkChange %"PRIx64"/%"PRIu32" --> %"PRIx64"/%"PRIu32" (%s).",
			le.dpsrc.as_host(), le.sport, le.dpdst.as_host(), le.dport, le.action == Link_event::ADD ? "add" : "remove");

	switch (le.action) {
		case Link_event::ADD : {
			handleLinkAdd(DpLink(DpPort(le.dpsrc, le.sport), DpPort(le.dpdst, le.dport)));
			break;
		}
		case Link_event::REMOVE : {
			/* only process link events; others are covered in other events */
			if (le.reason == Link_event::LINK) {
				handleLinkRemove(DpLink(DpPort(le.dpsrc, le.sport), DpPort(le.dpdst, le.dport)));
			}
			break;
		}
	}
	return CONTINUE;
}


void
Topology::sendJsonDpEvent(const datapathid& dpid, bool join) {
	if (!jsonDisp->isConnected()) {
		return;
	}

	json_object *msg = JsonDispatcher::jsonInit(MESSENGER_NAME, "dp");
	json_dict *msgDict = static_cast<json_dict *>(msg->object);

	msgDict->insert(make_pair("dp", dpToJson(dpid)));
	msgDict->insert(make_pair("event", JsonDispatcher::jsonString(join ? "join" : "leave")));

	jsonDisp->send(auto_ptr<json_object>(msg));
}

void
Topology::sendJsonLinkEvent(const DpLink& link, bool add) {
	if (!jsonDisp->isConnected()) {
		return;
	}

	json_object *msg = JsonDispatcher::jsonInit(MESSENGER_NAME, "link");
	json_dict *msgDict = static_cast<json_dict *>(msg->object);

	msgDict->insert(make_pair("link", linkToJson(link)));
	msgDict->insert(make_pair("event", JsonDispatcher::jsonString(add ? "add" : "remove")));

	jsonDisp->send(auto_ptr<json_object>(msg));
}

auto_ptr<json_object>
Topology::handleJsonTopo(const json_dict* json) {
	lg.dbg("handleJsonDump");

	json_object *msg = JsonDispatcher::jsonInit(MESSENGER_NAME, "topo");
	json_dict *msgDict = static_cast<json_dict *>(msg->object);

	json_object *dps = new json_object(json_object::JSONT_ARRAY);
	json_array *dpsArr = new json_array();
	dps->object = dpsArr;

	for (DpidToNodeMap::iterator it = dpidToNode.begin(); it != dpidToNode.end(); ++it) {
		dpsArr->push_back(dpToJson(it->first));
	}

	msgDict->insert(make_pair("dps", dps));

	json_object *links = new json_object(json_object::JSONT_ARRAY);
	json_array *linksArr = new json_array();
	links->object = linksArr;

	for (LinkToArcMap::iterator it = linkToArc.begin(); it != linkToArc.end(); ++it) {
		linksArr->push_back(linkToJson(it->first));
	}

	msgDict->insert(make_pair("links", links));

	return auto_ptr<json_object>(msg);
}

json_object *
Topology::dpToJson(const datapathid &dpid) {
	json_object *obj = new json_object(json_object::JSONT_DICT);
	json_dict *objDict = new json_dict();
	obj->object = objDict;

	objDict->insert(make_pair("dpid", JsonDispatcher::jsonString(dpid.string())));

	return obj;
}

json_object *
Topology::portToJson(const DpPort &port) {
	json_object *obj = new json_object(json_object::JSONT_DICT);
	json_dict *objDict = new json_dict();
	obj->object = objDict;

	objDict->insert(make_pair("dpid", JsonDispatcher::jsonString(port.dpid.string())));
	objDict->insert(make_pair("port", JsonDispatcher::jsonInt(port.port)));

	return obj;
}

json_object *
Topology::linkToJson(const DpLink &link) {
	json_object *obj = new json_object(json_object::JSONT_DICT);
	json_dict *objDict = new json_dict();
	obj->object = objDict;

	objDict->insert(make_pair("src", portToJson(link.src)));
	objDict->insert(make_pair("dst", portToJson(link.dst)));

	return obj;
}


auto_ptr<json_object>
Topology::handleJsonDump(const json_dict* json) {
	lg.dbg("handleJsonDump");

	stringstream ss;

	DigraphWriter<ListDigraph>(topo, ss)
	.nodeMap("dpid", nodeToDpid)
	.arcMap("link", arcToLink)
	.run();

	json_object *msg = JsonDispatcher::jsonInit(typeid(Topology).name(), "dump");
	json_dict *msgDict = static_cast<json_dict *>(msg->object);
    msgDict->insert(make_pair("data", JsonDispatcher::jsonString(ss.str())));

	return auto_ptr<json_object>(msg);
}


const ListDigraph&
Topology::getTopo() {
	return topo;
}

const Topology::NodeToDpidMap&
Topology::getNodeToDpid() {
	return nodeToDpid;
}

const Topology::ArcToLinkMap&
Topology::getArcToLink() {
	 return arcToLink;
}

const Topology::DpidToNodeMap&
Topology::getDpidToNode() {
	return dpidToNode;
}

const Topology::LinkToArcMap&
Topology::getLinkToArc() {
	return linkToArc;
}

const Topology::DpidToPortsMap&
Topology::getDpidToPorts() {
	return dpidToPorts;
}

const Topology::PortToLinksMap&
Topology::getPortToLinks() {
	return portToLinks;
}

const Topology::PortToPropsMap&
Topology::getPortToProps() {
	return portToProps;
}

const Topology::LinkToPropsMap&
Topology::getLinkToProps() {
	return linkToProps;
}

REGISTER_COMPONENT(Simple_component_factory<Topology>, Topology);

} // namespace ericsson

